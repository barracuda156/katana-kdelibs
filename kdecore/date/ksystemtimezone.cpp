/*
    This file is part of the KDE libraries
    Copyright (C) 2023 Ivailo Monev <xakepa10@gmail.com>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config-date.h"
#include "config-prefix.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QFileSystemWatcher>
#include <QElapsedTimer>

#include "kglobal.h"
#include "klocale.h"
#include "kdebug.h"
#include "ksystemtimezone.h"
#include "kde_file.h"

#include <sys/time.h>
#include <time.h>

typedef QPair<float, float> KZoneCoordinates;

static const QString s_localtime = QString::fromLatin1("/etc/localtime");

static const struct timezoneNameData {
    const char* name;
    const char* translation;
} timezoneNameTbl[] = {
    { "Africa/Abidjan", I18N_NOOP2("Timezone name", "Africa/Abidjan") },
    { "Africa/Accra", I18N_NOOP2("Timezone name", "Africa/Accra") },
    { "Africa/Addis_Ababa", I18N_NOOP2("Timezone name", "Africa/Addis_Ababa") },
    { "Africa/Algiers", I18N_NOOP2("Timezone name", "Africa/Algiers") },
    { "Africa/Asmara", I18N_NOOP2("Timezone name", "Africa/Asmara") },
    { "Africa/Bamako", I18N_NOOP2("Timezone name", "Africa/Bamako") },
    { "Africa/Bangui", I18N_NOOP2("Timezone name", "Africa/Bangui") },
    { "Africa/Banjul", I18N_NOOP2("Timezone name", "Africa/Banjul") },
    { "Africa/Bissau", I18N_NOOP2("Timezone name", "Africa/Bissau") },
    { "Africa/Blantyre", I18N_NOOP2("Timezone name", "Africa/Blantyre") },
    { "Africa/Brazzaville", I18N_NOOP2("Timezone name", "Africa/Brazzaville") },
    { "Africa/Bujumbura", I18N_NOOP2("Timezone name", "Africa/Bujumbura") },
    { "Africa/Cairo", I18N_NOOP2("Timezone name", "Africa/Cairo") },
    { "Africa/Casablanca", I18N_NOOP2("Timezone name", "Africa/Casablanca") },
    { "Africa/Ceuta", I18N_NOOP2("Timezone name", "Africa/Ceuta") },
    { "Africa/Conakry", I18N_NOOP2("Timezone name", "Africa/Conakry") },
    { "Africa/Dakar", I18N_NOOP2("Timezone name", "Africa/Dakar") },
    { "Africa/Dar_es_Salaam", I18N_NOOP2("Timezone name", "Africa/Dar_es_Salaam") },
    { "Africa/Djibouti", I18N_NOOP2("Timezone name", "Africa/Djibouti") },
    { "Africa/Douala", I18N_NOOP2("Timezone name", "Africa/Douala") },
    { "Africa/El_Aaiun", I18N_NOOP2("Timezone name", "Africa/El_Aaiun") },
    { "Africa/Freetown", I18N_NOOP2("Timezone name", "Africa/Freetown") },
    { "Africa/Gaborone", I18N_NOOP2("Timezone name", "Africa/Gaborone") },
    { "Africa/Harare", I18N_NOOP2("Timezone name", "Africa/Harare") },
    { "Africa/Johannesburg", I18N_NOOP2("Timezone name", "Africa/Johannesburg") },
    { "Africa/Juba", I18N_NOOP2("Timezone name", "Africa/Juba") },
    { "Africa/Kampala", I18N_NOOP2("Timezone name", "Africa/Kampala") },
    { "Africa/Khartoum", I18N_NOOP2("Timezone name", "Africa/Khartoum") },
    { "Africa/Kigali", I18N_NOOP2("Timezone name", "Africa/Kigali") },
    { "Africa/Kinshasa", I18N_NOOP2("Timezone name", "Africa/Kinshasa") },
    { "Africa/Lagos", I18N_NOOP2("Timezone name", "Africa/Lagos") },
    { "Africa/Libreville", I18N_NOOP2("Timezone name", "Africa/Libreville") },
    { "Africa/Lome", I18N_NOOP2("Timezone name", "Africa/Lome") },
    { "Africa/Luanda", I18N_NOOP2("Timezone name", "Africa/Luanda") },
    { "Africa/Lubumbashi", I18N_NOOP2("Timezone name", "Africa/Lubumbashi") },
    { "Africa/Lusaka", I18N_NOOP2("Timezone name", "Africa/Lusaka") },
    { "Africa/Malabo", I18N_NOOP2("Timezone name", "Africa/Malabo") },
    { "Africa/Maputo", I18N_NOOP2("Timezone name", "Africa/Maputo") },
    { "Africa/Maseru", I18N_NOOP2("Timezone name", "Africa/Maseru") },
    { "Africa/Mbabane", I18N_NOOP2("Timezone name", "Africa/Mbabane") },
    { "Africa/Mogadishu", I18N_NOOP2("Timezone name", "Africa/Mogadishu") },
    { "Africa/Monrovia", I18N_NOOP2("Timezone name", "Africa/Monrovia") },
    { "Africa/Nairobi", I18N_NOOP2("Timezone name", "Africa/Nairobi") },
    { "Africa/Ndjamena", I18N_NOOP2("Timezone name", "Africa/Ndjamena") },
    { "Africa/Niamey", I18N_NOOP2("Timezone name", "Africa/Niamey") },
    { "Africa/Nouakchott", I18N_NOOP2("Timezone name", "Africa/Nouakchott") },
    { "Africa/Ouagadougou", I18N_NOOP2("Timezone name", "Africa/Ouagadougou") },
    { "Africa/Porto-Novo", I18N_NOOP2("Timezone name", "Africa/Porto-Novo") },
    { "Africa/Sao_Tome", I18N_NOOP2("Timezone name", "Africa/Sao_Tome") },
    { "Africa/Tripoli", I18N_NOOP2("Timezone name", "Africa/Tripoli") },
    { "Africa/Tunis", I18N_NOOP2("Timezone name", "Africa/Tunis") },
    { "Africa/Windhoek", I18N_NOOP2("Timezone name", "Africa/Windhoek") },
    { "America/Adak", I18N_NOOP2("Timezone name", "America/Adak") },
    { "America/Anchorage", I18N_NOOP2("Timezone name", "America/Anchorage") },
    { "America/Anguilla", I18N_NOOP2("Timezone name", "America/Anguilla") },
    { "America/Antigua", I18N_NOOP2("Timezone name", "America/Antigua") },
    { "America/Araguaina", I18N_NOOP2("Timezone name", "America/Araguaina") },
    { "America/Argentina/Buenos_Aires", I18N_NOOP2("Timezone name", "America/Argentina/Buenos_Aires") },
    { "America/Argentina/Catamarca", I18N_NOOP2("Timezone name", "America/Argentina/Catamarca") },
    { "America/Argentina/Cordoba", I18N_NOOP2("Timezone name", "America/Argentina/Cordoba") },
    { "America/Argentina/Jujuy", I18N_NOOP2("Timezone name", "America/Argentina/Jujuy") },
    { "America/Argentina/La_Rioja", I18N_NOOP2("Timezone name", "America/Argentina/La_Rioja") },
    { "America/Argentina/Mendoza", I18N_NOOP2("Timezone name", "America/Argentina/Mendoza") },
    { "America/Argentina/Rio_Gallegos", I18N_NOOP2("Timezone name", "America/Argentina/Rio_Gallegos") },
    { "America/Argentina/Salta", I18N_NOOP2("Timezone name", "America/Argentina/Salta") },
    { "America/Argentina/San_Juan", I18N_NOOP2("Timezone name", "America/Argentina/San_Juan") },
    { "America/Argentina/San_Luis", I18N_NOOP2("Timezone name", "America/Argentina/San_Luis") },
    { "America/Argentina/Tucuman", I18N_NOOP2("Timezone name", "America/Argentina/Tucuman") },
    { "America/Argentina/Ushuaia", I18N_NOOP2("Timezone name", "America/Argentina/Ushuaia") },
    { "America/Aruba", I18N_NOOP2("Timezone name", "America/Aruba") },
    { "America/Asuncion", I18N_NOOP2("Timezone name", "America/Asuncion") },
    { "America/Atikokan", I18N_NOOP2("Timezone name", "America/Atikokan") },
    { "America/Bahia", I18N_NOOP2("Timezone name", "America/Bahia") },
    { "America/Bahia_Banderas", I18N_NOOP2("Timezone name", "America/Bahia_Banderas") },
    { "America/Barbados", I18N_NOOP2("Timezone name", "America/Barbados") },
    { "America/Belem", I18N_NOOP2("Timezone name", "America/Belem") },
    { "America/Belize", I18N_NOOP2("Timezone name", "America/Belize") },
    { "America/Blanc-Sablon", I18N_NOOP2("Timezone name", "America/Blanc-Sablon") },
    { "America/Boa_Vista", I18N_NOOP2("Timezone name", "America/Boa_Vista") },
    { "America/Bogota", I18N_NOOP2("Timezone name", "America/Bogota") },
    { "America/Boise", I18N_NOOP2("Timezone name", "America/Boise") },
    { "America/Cambridge_Bay", I18N_NOOP2("Timezone name", "America/Cambridge_Bay") },
    { "America/Campo_Grande", I18N_NOOP2("Timezone name", "America/Campo_Grande") },
    { "America/Cancun", I18N_NOOP2("Timezone name", "America/Cancun") },
    { "America/Caracas", I18N_NOOP2("Timezone name", "America/Caracas") },
    { "America/Cayenne", I18N_NOOP2("Timezone name", "America/Cayenne") },
    { "America/Cayman", I18N_NOOP2("Timezone name", "America/Cayman") },
    { "America/Chicago", I18N_NOOP2("Timezone name", "America/Chicago") },
    { "America/Chihuahua", I18N_NOOP2("Timezone name", "America/Chihuahua") },
    { "America/Ciudad_Juarez", I18N_NOOP2("Timezone name", "America/Ciudad_Juarez") },
    { "America/Costa_Rica", I18N_NOOP2("Timezone name", "America/Costa_Rica") },
    { "America/Creston", I18N_NOOP2("Timezone name", "America/Creston") },
    { "America/Cuiaba", I18N_NOOP2("Timezone name", "America/Cuiaba") },
    { "America/Curacao", I18N_NOOP2("Timezone name", "America/Curacao") },
    { "America/Danmarkshavn", I18N_NOOP2("Timezone name", "America/Danmarkshavn") },
    { "America/Dawson", I18N_NOOP2("Timezone name", "America/Dawson") },
    { "America/Dawson_Creek", I18N_NOOP2("Timezone name", "America/Dawson_Creek") },
    { "America/Denver", I18N_NOOP2("Timezone name", "America/Denver") },
    { "America/Detroit", I18N_NOOP2("Timezone name", "America/Detroit") },
    { "America/Dominica", I18N_NOOP2("Timezone name", "America/Dominica") },
    { "America/Edmonton", I18N_NOOP2("Timezone name", "America/Edmonton") },
    { "America/Eirunepe", I18N_NOOP2("Timezone name", "America/Eirunepe") },
    { "America/El_Salvador", I18N_NOOP2("Timezone name", "America/El_Salvador") },
    { "America/Fort_Nelson", I18N_NOOP2("Timezone name", "America/Fort_Nelson") },
    { "America/Fortaleza", I18N_NOOP2("Timezone name", "America/Fortaleza") },
    { "America/Glace_Bay", I18N_NOOP2("Timezone name", "America/Glace_Bay") },
    { "America/Goose_Bay", I18N_NOOP2("Timezone name", "America/Goose_Bay") },
    { "America/Grand_Turk", I18N_NOOP2("Timezone name", "America/Grand_Turk") },
    { "America/Grenada", I18N_NOOP2("Timezone name", "America/Grenada") },
    { "America/Guadeloupe", I18N_NOOP2("Timezone name", "America/Guadeloupe") },
    { "America/Guatemala", I18N_NOOP2("Timezone name", "America/Guatemala") },
    { "America/Guayaquil", I18N_NOOP2("Timezone name", "America/Guayaquil") },
    { "America/Guyana", I18N_NOOP2("Timezone name", "America/Guyana") },
    { "America/Halifax", I18N_NOOP2("Timezone name", "America/Halifax") },
    { "America/Havana", I18N_NOOP2("Timezone name", "America/Havana") },
    { "America/Hermosillo", I18N_NOOP2("Timezone name", "America/Hermosillo") },
    { "America/Indiana/Indianapolis", I18N_NOOP2("Timezone name", "America/Indiana/Indianapolis") },
    { "America/Indiana/Knox", I18N_NOOP2("Timezone name", "America/Indiana/Knox") },
    { "America/Indiana/Marengo", I18N_NOOP2("Timezone name", "America/Indiana/Marengo") },
    { "America/Indiana/Petersburg", I18N_NOOP2("Timezone name", "America/Indiana/Petersburg") },
    { "America/Indiana/Tell_City", I18N_NOOP2("Timezone name", "America/Indiana/Tell_City") },
    { "America/Indiana/Vevay", I18N_NOOP2("Timezone name", "America/Indiana/Vevay") },
    { "America/Indiana/Vincennes", I18N_NOOP2("Timezone name", "America/Indiana/Vincennes") },
    { "America/Indiana/Winamac", I18N_NOOP2("Timezone name", "America/Indiana/Winamac") },
    { "America/Inuvik", I18N_NOOP2("Timezone name", "America/Inuvik") },
    { "America/Iqaluit", I18N_NOOP2("Timezone name", "America/Iqaluit") },
    { "America/Jamaica", I18N_NOOP2("Timezone name", "America/Jamaica") },
    { "America/Juneau", I18N_NOOP2("Timezone name", "America/Juneau") },
    { "America/Kentucky/Louisville", I18N_NOOP2("Timezone name", "America/Kentucky/Louisville") },
    { "America/Kentucky/Monticello", I18N_NOOP2("Timezone name", "America/Kentucky/Monticello") },
    { "America/Kralendijk", I18N_NOOP2("Timezone name", "America/Kralendijk") },
    { "America/La_Paz", I18N_NOOP2("Timezone name", "America/La_Paz") },
    { "America/Lima", I18N_NOOP2("Timezone name", "America/Lima") },
    { "America/Los_Angeles", I18N_NOOP2("Timezone name", "America/Los_Angeles") },
    { "America/Lower_Princes", I18N_NOOP2("Timezone name", "America/Lower_Princes") },
    { "America/Maceio", I18N_NOOP2("Timezone name", "America/Maceio") },
    { "America/Managua", I18N_NOOP2("Timezone name", "America/Managua") },
    { "America/Manaus", I18N_NOOP2("Timezone name", "America/Manaus") },
    { "America/Marigot", I18N_NOOP2("Timezone name", "America/Marigot") },
    { "America/Martinique", I18N_NOOP2("Timezone name", "America/Martinique") },
    { "America/Matamoros", I18N_NOOP2("Timezone name", "America/Matamoros") },
    { "America/Mazatlan", I18N_NOOP2("Timezone name", "America/Mazatlan") },
    { "America/Menominee", I18N_NOOP2("Timezone name", "America/Menominee") },
    { "America/Merida", I18N_NOOP2("Timezone name", "America/Merida") },
    { "America/Metlakatla", I18N_NOOP2("Timezone name", "America/Metlakatla") },
    { "America/Mexico_City", I18N_NOOP2("Timezone name", "America/Mexico_City") },
    { "America/Miquelon", I18N_NOOP2("Timezone name", "America/Miquelon") },
    { "America/Moncton", I18N_NOOP2("Timezone name", "America/Moncton") },
    { "America/Monterrey", I18N_NOOP2("Timezone name", "America/Monterrey") },
    { "America/Montevideo", I18N_NOOP2("Timezone name", "America/Montevideo") },
    { "America/Montserrat", I18N_NOOP2("Timezone name", "America/Montserrat") },
    { "America/Nassau", I18N_NOOP2("Timezone name", "America/Nassau") },
    { "America/New_York", I18N_NOOP2("Timezone name", "America/New_York") },
    { "America/Nome", I18N_NOOP2("Timezone name", "America/Nome") },
    { "America/Noronha", I18N_NOOP2("Timezone name", "America/Noronha") },
    { "America/North_Dakota/Beulah", I18N_NOOP2("Timezone name", "America/North_Dakota/Beulah") },
    { "America/North_Dakota/Center", I18N_NOOP2("Timezone name", "America/North_Dakota/Center") },
    { "America/North_Dakota/New_Salem", I18N_NOOP2("Timezone name", "America/North_Dakota/New_Salem") },
    { "America/Nuuk", I18N_NOOP2("Timezone name", "America/Nuuk") },
    { "America/Ojinaga", I18N_NOOP2("Timezone name", "America/Ojinaga") },
    { "America/Panama", I18N_NOOP2("Timezone name", "America/Panama") },
    { "America/Paramaribo", I18N_NOOP2("Timezone name", "America/Paramaribo") },
    { "America/Phoenix", I18N_NOOP2("Timezone name", "America/Phoenix") },
    { "America/Port-au-Prince", I18N_NOOP2("Timezone name", "America/Port-au-Prince") },
    { "America/Port_of_Spain", I18N_NOOP2("Timezone name", "America/Port_of_Spain") },
    { "America/Porto_Velho", I18N_NOOP2("Timezone name", "America/Porto_Velho") },
    { "America/Puerto_Rico", I18N_NOOP2("Timezone name", "America/Puerto_Rico") },
    { "America/Punta_Arenas", I18N_NOOP2("Timezone name", "America/Punta_Arenas") },
    { "America/Rankin_Inlet", I18N_NOOP2("Timezone name", "America/Rankin_Inlet") },
    { "America/Recife", I18N_NOOP2("Timezone name", "America/Recife") },
    { "America/Regina", I18N_NOOP2("Timezone name", "America/Regina") },
    { "America/Resolute", I18N_NOOP2("Timezone name", "America/Resolute") },
    { "America/Rio_Branco", I18N_NOOP2("Timezone name", "America/Rio_Branco") },
    { "America/Santarem", I18N_NOOP2("Timezone name", "America/Santarem") },
    { "America/Santiago", I18N_NOOP2("Timezone name", "America/Santiago") },
    { "America/Santo_Domingo", I18N_NOOP2("Timezone name", "America/Santo_Domingo") },
    { "America/Sao_Paulo", I18N_NOOP2("Timezone name", "America/Sao_Paulo") },
    { "America/Scoresbysund", I18N_NOOP2("Timezone name", "America/Scoresbysund") },
    { "America/Sitka", I18N_NOOP2("Timezone name", "America/Sitka") },
    { "America/St_Barthelemy", I18N_NOOP2("Timezone name", "America/St_Barthelemy") },
    { "America/St_Johns", I18N_NOOP2("Timezone name", "America/St_Johns") },
    { "America/St_Kitts", I18N_NOOP2("Timezone name", "America/St_Kitts") },
    { "America/St_Lucia", I18N_NOOP2("Timezone name", "America/St_Lucia") },
    { "America/St_Thomas", I18N_NOOP2("Timezone name", "America/St_Thomas") },
    { "America/St_Vincent", I18N_NOOP2("Timezone name", "America/St_Vincent") },
    { "America/Swift_Current", I18N_NOOP2("Timezone name", "America/Swift_Current") },
    { "America/Tegucigalpa", I18N_NOOP2("Timezone name", "America/Tegucigalpa") },
    { "America/Thule", I18N_NOOP2("Timezone name", "America/Thule") },
    { "America/Tijuana", I18N_NOOP2("Timezone name", "America/Tijuana") },
    { "America/Toronto", I18N_NOOP2("Timezone name", "America/Toronto") },
    { "America/Tortola", I18N_NOOP2("Timezone name", "America/Tortola") },
    { "America/Vancouver", I18N_NOOP2("Timezone name", "America/Vancouver") },
    { "America/Whitehorse", I18N_NOOP2("Timezone name", "America/Whitehorse") },
    { "America/Winnipeg", I18N_NOOP2("Timezone name", "America/Winnipeg") },
    { "America/Yakutat", I18N_NOOP2("Timezone name", "America/Yakutat") },
    { "Antarctica/Casey", I18N_NOOP2("Timezone name", "Antarctica/Casey") },
    { "Antarctica/Davis", I18N_NOOP2("Timezone name", "Antarctica/Davis") },
    { "Antarctica/DumontDUrville", I18N_NOOP2("Timezone name", "Antarctica/DumontDUrville") },
    { "Antarctica/Macquarie", I18N_NOOP2("Timezone name", "Antarctica/Macquarie") },
    { "Antarctica/Mawson", I18N_NOOP2("Timezone name", "Antarctica/Mawson") },
    { "Antarctica/McMurdo", I18N_NOOP2("Timezone name", "Antarctica/McMurdo") },
    { "Antarctica/Palmer", I18N_NOOP2("Timezone name", "Antarctica/Palmer") },
    { "Antarctica/Rothera", I18N_NOOP2("Timezone name", "Antarctica/Rothera") },
    { "Antarctica/Syowa", I18N_NOOP2("Timezone name", "Antarctica/Syowa") },
    { "Antarctica/Troll", I18N_NOOP2("Timezone name", "Antarctica/Troll") },
    { "Antarctica/Vostok", I18N_NOOP2("Timezone name", "Antarctica/Vostok") },
    { "Arctic/Longyearbyen", I18N_NOOP2("Timezone name", "Arctic/Longyearbyen") },
    { "Asia/Aden", I18N_NOOP2("Timezone name", "Asia/Aden") },
    { "Asia/Almaty", I18N_NOOP2("Timezone name", "Asia/Almaty") },
    { "Asia/Amman", I18N_NOOP2("Timezone name", "Asia/Amman") },
    { "Asia/Anadyr", I18N_NOOP2("Timezone name", "Asia/Anadyr") },
    { "Asia/Aqtau", I18N_NOOP2("Timezone name", "Asia/Aqtau") },
    { "Asia/Aqtobe", I18N_NOOP2("Timezone name", "Asia/Aqtobe") },
    { "Asia/Ashgabat", I18N_NOOP2("Timezone name", "Asia/Ashgabat") },
    { "Asia/Atyrau", I18N_NOOP2("Timezone name", "Asia/Atyrau") },
    { "Asia/Baghdad", I18N_NOOP2("Timezone name", "Asia/Baghdad") },
    { "Asia/Bahrain", I18N_NOOP2("Timezone name", "Asia/Bahrain") },
    { "Asia/Baku", I18N_NOOP2("Timezone name", "Asia/Baku") },
    { "Asia/Bangkok", I18N_NOOP2("Timezone name", "Asia/Bangkok") },
    { "Asia/Barnaul", I18N_NOOP2("Timezone name", "Asia/Barnaul") },
    { "Asia/Beirut", I18N_NOOP2("Timezone name", "Asia/Beirut") },
    { "Asia/Bishkek", I18N_NOOP2("Timezone name", "Asia/Bishkek") },
    { "Asia/Brunei", I18N_NOOP2("Timezone name", "Asia/Brunei") },
    { "Asia/Chita", I18N_NOOP2("Timezone name", "Asia/Chita") },
    { "Asia/Choibalsan", I18N_NOOP2("Timezone name", "Asia/Choibalsan") },
    { "Asia/Colombo", I18N_NOOP2("Timezone name", "Asia/Colombo") },
    { "Asia/Damascus", I18N_NOOP2("Timezone name", "Asia/Damascus") },
    { "Asia/Dhaka", I18N_NOOP2("Timezone name", "Asia/Dhaka") },
    { "Asia/Dili", I18N_NOOP2("Timezone name", "Asia/Dili") },
    { "Asia/Dubai", I18N_NOOP2("Timezone name", "Asia/Dubai") },
    { "Asia/Dushanbe", I18N_NOOP2("Timezone name", "Asia/Dushanbe") },
    { "Asia/Famagusta", I18N_NOOP2("Timezone name", "Asia/Famagusta") },
    { "Asia/Gaza", I18N_NOOP2("Timezone name", "Asia/Gaza") },
    { "Asia/Hebron", I18N_NOOP2("Timezone name", "Asia/Hebron") },
    { "Asia/Ho_Chi_Minh", I18N_NOOP2("Timezone name", "Asia/Ho_Chi_Minh") },
    { "Asia/Hong_Kong", I18N_NOOP2("Timezone name", "Asia/Hong_Kong") },
    { "Asia/Hovd", I18N_NOOP2("Timezone name", "Asia/Hovd") },
    { "Asia/Irkutsk", I18N_NOOP2("Timezone name", "Asia/Irkutsk") },
    { "Asia/Jakarta", I18N_NOOP2("Timezone name", "Asia/Jakarta") },
    { "Asia/Jayapura", I18N_NOOP2("Timezone name", "Asia/Jayapura") },
    { "Asia/Jerusalem", I18N_NOOP2("Timezone name", "Asia/Jerusalem") },
    { "Asia/Kabul", I18N_NOOP2("Timezone name", "Asia/Kabul") },
    { "Asia/Kamchatka", I18N_NOOP2("Timezone name", "Asia/Kamchatka") },
    { "Asia/Karachi", I18N_NOOP2("Timezone name", "Asia/Karachi") },
    { "Asia/Kathmandu", I18N_NOOP2("Timezone name", "Asia/Kathmandu") },
    { "Asia/Khandyga", I18N_NOOP2("Timezone name", "Asia/Khandyga") },
    { "Asia/Kolkata", I18N_NOOP2("Timezone name", "Asia/Kolkata") },
    { "Asia/Krasnoyarsk", I18N_NOOP2("Timezone name", "Asia/Krasnoyarsk") },
    { "Asia/Kuala_Lumpur", I18N_NOOP2("Timezone name", "Asia/Kuala_Lumpur") },
    { "Asia/Kuching", I18N_NOOP2("Timezone name", "Asia/Kuching") },
    { "Asia/Kuwait", I18N_NOOP2("Timezone name", "Asia/Kuwait") },
    { "Asia/Macau", I18N_NOOP2("Timezone name", "Asia/Macau") },
    { "Asia/Magadan", I18N_NOOP2("Timezone name", "Asia/Magadan") },
    { "Asia/Makassar", I18N_NOOP2("Timezone name", "Asia/Makassar") },
    { "Asia/Manila", I18N_NOOP2("Timezone name", "Asia/Manila") },
    { "Asia/Muscat", I18N_NOOP2("Timezone name", "Asia/Muscat") },
    { "Asia/Nicosia", I18N_NOOP2("Timezone name", "Asia/Nicosia") },
    { "Asia/Novokuznetsk", I18N_NOOP2("Timezone name", "Asia/Novokuznetsk") },
    { "Asia/Novosibirsk", I18N_NOOP2("Timezone name", "Asia/Novosibirsk") },
    { "Asia/Omsk", I18N_NOOP2("Timezone name", "Asia/Omsk") },
    { "Asia/Oral", I18N_NOOP2("Timezone name", "Asia/Oral") },
    { "Asia/Phnom_Penh", I18N_NOOP2("Timezone name", "Asia/Phnom_Penh") },
    { "Asia/Pontianak", I18N_NOOP2("Timezone name", "Asia/Pontianak") },
    { "Asia/Pyongyang", I18N_NOOP2("Timezone name", "Asia/Pyongyang") },
    { "Asia/Qatar", I18N_NOOP2("Timezone name", "Asia/Qatar") },
    { "Asia/Qostanay", I18N_NOOP2("Timezone name", "Asia/Qostanay") },
    { "Asia/Qyzylorda", I18N_NOOP2("Timezone name", "Asia/Qyzylorda") },
    { "Asia/Riyadh", I18N_NOOP2("Timezone name", "Asia/Riyadh") },
    { "Asia/Sakhalin", I18N_NOOP2("Timezone name", "Asia/Sakhalin") },
    { "Asia/Samarkand", I18N_NOOP2("Timezone name", "Asia/Samarkand") },
    { "Asia/Seoul", I18N_NOOP2("Timezone name", "Asia/Seoul") },
    { "Asia/Shanghai", I18N_NOOP2("Timezone name", "Asia/Shanghai") },
    { "Asia/Singapore", I18N_NOOP2("Timezone name", "Asia/Singapore") },
    { "Asia/Srednekolymsk", I18N_NOOP2("Timezone name", "Asia/Srednekolymsk") },
    { "Asia/Taipei", I18N_NOOP2("Timezone name", "Asia/Taipei") },
    { "Asia/Tashkent", I18N_NOOP2("Timezone name", "Asia/Tashkent") },
    { "Asia/Tbilisi", I18N_NOOP2("Timezone name", "Asia/Tbilisi") },
    { "Asia/Tehran", I18N_NOOP2("Timezone name", "Asia/Tehran") },
    { "Asia/Thimphu", I18N_NOOP2("Timezone name", "Asia/Thimphu") },
    { "Asia/Tokyo", I18N_NOOP2("Timezone name", "Asia/Tokyo") },
    { "Asia/Tomsk", I18N_NOOP2("Timezone name", "Asia/Tomsk") },
    { "Asia/Ulaanbaatar", I18N_NOOP2("Timezone name", "Asia/Ulaanbaatar") },
    { "Asia/Urumqi", I18N_NOOP2("Timezone name", "Asia/Urumqi") },
    { "Asia/Ust-Nera", I18N_NOOP2("Timezone name", "Asia/Ust-Nera") },
    { "Asia/Vientiane", I18N_NOOP2("Timezone name", "Asia/Vientiane") },
    { "Asia/Vladivostok", I18N_NOOP2("Timezone name", "Asia/Vladivostok") },
    { "Asia/Yakutsk", I18N_NOOP2("Timezone name", "Asia/Yakutsk") },
    { "Asia/Yangon", I18N_NOOP2("Timezone name", "Asia/Yangon") },
    { "Asia/Yekaterinburg", I18N_NOOP2("Timezone name", "Asia/Yekaterinburg") },
    { "Asia/Yerevan", I18N_NOOP2("Timezone name", "Asia/Yerevan") },
    { "Atlantic/Azores", I18N_NOOP2("Timezone name", "Atlantic/Azores") },
    { "Atlantic/Bermuda", I18N_NOOP2("Timezone name", "Atlantic/Bermuda") },
    { "Atlantic/Canary", I18N_NOOP2("Timezone name", "Atlantic/Canary") },
    { "Atlantic/Cape_Verde", I18N_NOOP2("Timezone name", "Atlantic/Cape_Verde") },
    { "Atlantic/Faroe", I18N_NOOP2("Timezone name", "Atlantic/Faroe") },
    { "Atlantic/Madeira", I18N_NOOP2("Timezone name", "Atlantic/Madeira") },
    { "Atlantic/Reykjavik", I18N_NOOP2("Timezone name", "Atlantic/Reykjavik") },
    { "Atlantic/South_Georgia", I18N_NOOP2("Timezone name", "Atlantic/South_Georgia") },
    { "Atlantic/St_Helena", I18N_NOOP2("Timezone name", "Atlantic/St_Helena") },
    { "Atlantic/Stanley", I18N_NOOP2("Timezone name", "Atlantic/Stanley") },
    { "Australia/Adelaide", I18N_NOOP2("Timezone name", "Australia/Adelaide") },
    { "Australia/Brisbane", I18N_NOOP2("Timezone name", "Australia/Brisbane") },
    { "Australia/Broken_Hill", I18N_NOOP2("Timezone name", "Australia/Broken_Hill") },
    { "Australia/Darwin", I18N_NOOP2("Timezone name", "Australia/Darwin") },
    { "Australia/Eucla", I18N_NOOP2("Timezone name", "Australia/Eucla") },
    { "Australia/Hobart", I18N_NOOP2("Timezone name", "Australia/Hobart") },
    { "Australia/Lindeman", I18N_NOOP2("Timezone name", "Australia/Lindeman") },
    { "Australia/Lord_Howe", I18N_NOOP2("Timezone name", "Australia/Lord_Howe") },
    { "Australia/Melbourne", I18N_NOOP2("Timezone name", "Australia/Melbourne") },
    { "Australia/Perth", I18N_NOOP2("Timezone name", "Australia/Perth") },
    { "Australia/Sydney", I18N_NOOP2("Timezone name", "Australia/Sydney") },
    { "Europe/Amsterdam", I18N_NOOP2("Timezone name", "Europe/Amsterdam") },
    { "Europe/Andorra", I18N_NOOP2("Timezone name", "Europe/Andorra") },
    { "Europe/Astrakhan", I18N_NOOP2("Timezone name", "Europe/Astrakhan") },
    { "Europe/Athens", I18N_NOOP2("Timezone name", "Europe/Athens") },
    { "Europe/Belgrade", I18N_NOOP2("Timezone name", "Europe/Belgrade") },
    { "Europe/Berlin", I18N_NOOP2("Timezone name", "Europe/Berlin") },
    { "Europe/Bratislava", I18N_NOOP2("Timezone name", "Europe/Bratislava") },
    { "Europe/Brussels", I18N_NOOP2("Timezone name", "Europe/Brussels") },
    { "Europe/Bucharest", I18N_NOOP2("Timezone name", "Europe/Bucharest") },
    { "Europe/Budapest", I18N_NOOP2("Timezone name", "Europe/Budapest") },
    { "Europe/Busingen", I18N_NOOP2("Timezone name", "Europe/Busingen") },
    { "Europe/Chisinau", I18N_NOOP2("Timezone name", "Europe/Chisinau") },
    { "Europe/Copenhagen", I18N_NOOP2("Timezone name", "Europe/Copenhagen") },
    { "Europe/Dublin", I18N_NOOP2("Timezone name", "Europe/Dublin") },
    { "Europe/Gibraltar", I18N_NOOP2("Timezone name", "Europe/Gibraltar") },
    { "Europe/Guernsey", I18N_NOOP2("Timezone name", "Europe/Guernsey") },
    { "Europe/Helsinki", I18N_NOOP2("Timezone name", "Europe/Helsinki") },
    { "Europe/Isle_of_Man", I18N_NOOP2("Timezone name", "Europe/Isle_of_Man") },
    { "Europe/Istanbul", I18N_NOOP2("Timezone name", "Europe/Istanbul") },
    { "Europe/Jersey", I18N_NOOP2("Timezone name", "Europe/Jersey") },
    { "Europe/Kaliningrad", I18N_NOOP2("Timezone name", "Europe/Kaliningrad") },
    { "Europe/Kirov", I18N_NOOP2("Timezone name", "Europe/Kirov") },
    { "Europe/Kyiv", I18N_NOOP2("Timezone name", "Europe/Kyiv") },
    { "Europe/Lisbon", I18N_NOOP2("Timezone name", "Europe/Lisbon") },
    { "Europe/Ljubljana", I18N_NOOP2("Timezone name", "Europe/Ljubljana") },
    { "Europe/London", I18N_NOOP2("Timezone name", "Europe/London") },
    { "Europe/Luxembourg", I18N_NOOP2("Timezone name", "Europe/Luxembourg") },
    { "Europe/Madrid", I18N_NOOP2("Timezone name", "Europe/Madrid") },
    { "Europe/Malta", I18N_NOOP2("Timezone name", "Europe/Malta") },
    { "Europe/Mariehamn", I18N_NOOP2("Timezone name", "Europe/Mariehamn") },
    { "Europe/Minsk", I18N_NOOP2("Timezone name", "Europe/Minsk") },
    { "Europe/Monaco", I18N_NOOP2("Timezone name", "Europe/Monaco") },
    { "Europe/Moscow", I18N_NOOP2("Timezone name", "Europe/Moscow") },
    { "Europe/Oslo", I18N_NOOP2("Timezone name", "Europe/Oslo") },
    { "Europe/Paris", I18N_NOOP2("Timezone name", "Europe/Paris") },
    { "Europe/Podgorica", I18N_NOOP2("Timezone name", "Europe/Podgorica") },
    { "Europe/Prague", I18N_NOOP2("Timezone name", "Europe/Prague") },
    { "Europe/Riga", I18N_NOOP2("Timezone name", "Europe/Riga") },
    { "Europe/Rome", I18N_NOOP2("Timezone name", "Europe/Rome") },
    { "Europe/Samara", I18N_NOOP2("Timezone name", "Europe/Samara") },
    { "Europe/San_Marino", I18N_NOOP2("Timezone name", "Europe/San_Marino") },
    { "Europe/Sarajevo", I18N_NOOP2("Timezone name", "Europe/Sarajevo") },
    { "Europe/Saratov", I18N_NOOP2("Timezone name", "Europe/Saratov") },
    { "Europe/Simferopol", I18N_NOOP2("Timezone name", "Europe/Simferopol") },
    { "Europe/Skopje", I18N_NOOP2("Timezone name", "Europe/Skopje") },
    { "Europe/Sofia", I18N_NOOP2("Timezone name", "Europe/Sofia") },
    { "Europe/Stockholm", I18N_NOOP2("Timezone name", "Europe/Stockholm") },
    { "Europe/Tallinn", I18N_NOOP2("Timezone name", "Europe/Tallinn") },
    { "Europe/Tirane", I18N_NOOP2("Timezone name", "Europe/Tirane") },
    { "Europe/Ulyanovsk", I18N_NOOP2("Timezone name", "Europe/Ulyanovsk") },
    { "Europe/Vaduz", I18N_NOOP2("Timezone name", "Europe/Vaduz") },
    { "Europe/Vatican", I18N_NOOP2("Timezone name", "Europe/Vatican") },
    { "Europe/Vienna", I18N_NOOP2("Timezone name", "Europe/Vienna") },
    { "Europe/Vilnius", I18N_NOOP2("Timezone name", "Europe/Vilnius") },
    { "Europe/Volgograd", I18N_NOOP2("Timezone name", "Europe/Volgograd") },
    { "Europe/Warsaw", I18N_NOOP2("Timezone name", "Europe/Warsaw") },
    { "Europe/Zagreb", I18N_NOOP2("Timezone name", "Europe/Zagreb") },
    { "Europe/Zurich", I18N_NOOP2("Timezone name", "Europe/Zurich") },
    { "Indian/Antananarivo", I18N_NOOP2("Timezone name", "Indian/Antananarivo") },
    { "Indian/Chagos", I18N_NOOP2("Timezone name", "Indian/Chagos") },
    { "Indian/Christmas", I18N_NOOP2("Timezone name", "Indian/Christmas") },
    { "Indian/Cocos", I18N_NOOP2("Timezone name", "Indian/Cocos") },
    { "Indian/Comoro", I18N_NOOP2("Timezone name", "Indian/Comoro") },
    { "Indian/Kerguelen", I18N_NOOP2("Timezone name", "Indian/Kerguelen") },
    { "Indian/Mahe", I18N_NOOP2("Timezone name", "Indian/Mahe") },
    { "Indian/Maldives", I18N_NOOP2("Timezone name", "Indian/Maldives") },
    { "Indian/Mauritius", I18N_NOOP2("Timezone name", "Indian/Mauritius") },
    { "Indian/Mayotte", I18N_NOOP2("Timezone name", "Indian/Mayotte") },
    { "Indian/Reunion", I18N_NOOP2("Timezone name", "Indian/Reunion") },
    { "Pacific/Apia", I18N_NOOP2("Timezone name", "Pacific/Apia") },
    { "Pacific/Auckland", I18N_NOOP2("Timezone name", "Pacific/Auckland") },
    { "Pacific/Bougainville", I18N_NOOP2("Timezone name", "Pacific/Bougainville") },
    { "Pacific/Chatham", I18N_NOOP2("Timezone name", "Pacific/Chatham") },
    { "Pacific/Chuuk", I18N_NOOP2("Timezone name", "Pacific/Chuuk") },
    { "Pacific/Easter", I18N_NOOP2("Timezone name", "Pacific/Easter") },
    { "Pacific/Efate", I18N_NOOP2("Timezone name", "Pacific/Efate") },
    { "Pacific/Fakaofo", I18N_NOOP2("Timezone name", "Pacific/Fakaofo") },
    { "Pacific/Fiji", I18N_NOOP2("Timezone name", "Pacific/Fiji") },
    { "Pacific/Funafuti", I18N_NOOP2("Timezone name", "Pacific/Funafuti") },
    { "Pacific/Galapagos", I18N_NOOP2("Timezone name", "Pacific/Galapagos") },
    { "Pacific/Gambier", I18N_NOOP2("Timezone name", "Pacific/Gambier") },
    { "Pacific/Guadalcanal", I18N_NOOP2("Timezone name", "Pacific/Guadalcanal") },
    { "Pacific/Guam", I18N_NOOP2("Timezone name", "Pacific/Guam") },
    { "Pacific/Honolulu", I18N_NOOP2("Timezone name", "Pacific/Honolulu") },
    { "Pacific/Kanton", I18N_NOOP2("Timezone name", "Pacific/Kanton") },
    { "Pacific/Kiritimati", I18N_NOOP2("Timezone name", "Pacific/Kiritimati") },
    { "Pacific/Kosrae", I18N_NOOP2("Timezone name", "Pacific/Kosrae") },
    { "Pacific/Kwajalein", I18N_NOOP2("Timezone name", "Pacific/Kwajalein") },
    { "Pacific/Majuro", I18N_NOOP2("Timezone name", "Pacific/Majuro") },
    { "Pacific/Marquesas", I18N_NOOP2("Timezone name", "Pacific/Marquesas") },
    { "Pacific/Midway", I18N_NOOP2("Timezone name", "Pacific/Midway") },
    { "Pacific/Nauru", I18N_NOOP2("Timezone name", "Pacific/Nauru") },
    { "Pacific/Niue", I18N_NOOP2("Timezone name", "Pacific/Niue") },
    { "Pacific/Norfolk", I18N_NOOP2("Timezone name", "Pacific/Norfolk") },
    { "Pacific/Noumea", I18N_NOOP2("Timezone name", "Pacific/Noumea") },
    { "Pacific/Pago_Pago", I18N_NOOP2("Timezone name", "Pacific/Pago_Pago") },
    { "Pacific/Palau", I18N_NOOP2("Timezone name", "Pacific/Palau") },
    { "Pacific/Pitcairn", I18N_NOOP2("Timezone name", "Pacific/Pitcairn") },
    { "Pacific/Pohnpei", I18N_NOOP2("Timezone name", "Pacific/Pohnpei") },
    { "Pacific/Port_Moresby", I18N_NOOP2("Timezone name", "Pacific/Port_Moresby") },
    { "Pacific/Rarotonga", I18N_NOOP2("Timezone name", "Pacific/Rarotonga") },
    { "Pacific/Saipan", I18N_NOOP2("Timezone name", "Pacific/Saipan") },
    { "Pacific/Tahiti", I18N_NOOP2("Timezone name", "Pacific/Tahiti") },
    { "Pacific/Tarawa", I18N_NOOP2("Timezone name", "Pacific/Tarawa") },
    { "Pacific/Tongatapu", I18N_NOOP2("Timezone name", "Pacific/Tongatapu") },
    { "Pacific/Wake", I18N_NOOP2("Timezone name", "Pacific/Wake") },
    { "Pacific/Wallis", I18N_NOOP2("Timezone name", "Pacific/Wallis") },
};
static const qint16 timezoneNameTblSize = sizeof(timezoneNameTbl) / sizeof(timezoneNameData);

static const struct timezoneCommentData {
    const char* name;
    const char* translation;
} timezoneCommentTbl[] = {
    { "Africa/Ceuta", I18N_NOOP2("Timezone comment", "Ceuta, Melilla") },
    { "Africa/Kinshasa", I18N_NOOP2("Timezone comment", "Dem. Rep. of Congo (west)") },
    { "Africa/Lubumbashi", I18N_NOOP2("Timezone comment", "Dem. Rep. of Congo (east)") },
    { "America/Adak", I18N_NOOP2("Timezone comment", "Alaska - western Aleutians") },
    { "America/Anchorage", I18N_NOOP2("Timezone comment", "Alaska (most areas)") },
    { "America/Araguaina", I18N_NOOP2("Timezone comment", "Tocantins") },
    { "America/Argentina/Buenos_Aires", I18N_NOOP2("Timezone comment", "Buenos Aires (BA, CF)") },
    { "America/Argentina/Catamarca", I18N_NOOP2("Timezone comment", "Catamarca (CT), Chubut (CH)") },
    { "America/Argentina/Cordoba", I18N_NOOP2("Timezone comment", "Argentina (most areas: CB, CC, CN, ER, FM, MN, SE, SF)") },
    { "America/Argentina/Jujuy", I18N_NOOP2("Timezone comment", "Jujuy (JY)") },
    { "America/Argentina/La_Rioja", I18N_NOOP2("Timezone comment", "La Rioja (LR)") },
    { "America/Argentina/Mendoza", I18N_NOOP2("Timezone comment", "Mendoza (MZ)") },
    { "America/Argentina/Rio_Gallegos", I18N_NOOP2("Timezone comment", "Santa Cruz (SC)") },
    { "America/Argentina/Salta", I18N_NOOP2("Timezone comment", "Salta (SA, LP, NQ, RN)") },
    { "America/Argentina/San_Juan", I18N_NOOP2("Timezone comment", "San Juan (SJ)") },
    { "America/Argentina/San_Luis", I18N_NOOP2("Timezone comment", "San Luis (SL)") },
    { "America/Argentina/Tucuman", I18N_NOOP2("Timezone comment", "Tucuman (TM)") },
    { "America/Argentina/Ushuaia", I18N_NOOP2("Timezone comment", "Tierra del Fuego (TF)") },
    { "America/Atikokan", I18N_NOOP2("Timezone comment", "EST - ON (Atikokan), NU (Coral H)") },
    { "America/Bahia", I18N_NOOP2("Timezone comment", "Bahia") },
    { "America/Bahia_Banderas", I18N_NOOP2("Timezone comment", "Bahia de Banderas") },
    { "America/Belem", I18N_NOOP2("Timezone comment", "Para (east), Amapa") },
    { "America/Blanc-Sablon", I18N_NOOP2("Timezone comment", "AST - QC (Lower North Shore)") },
    { "America/Boa_Vista", I18N_NOOP2("Timezone comment", "Roraima") },
    { "America/Boise", I18N_NOOP2("Timezone comment", "Mountain - ID (south), OR (east)") },
    { "America/Cambridge_Bay", I18N_NOOP2("Timezone comment", "Mountain - NU (west)") },
    { "America/Campo_Grande", I18N_NOOP2("Timezone comment", "Mato Grosso do Sul") },
    { "America/Cancun", I18N_NOOP2("Timezone comment", "Quintana Roo") },
    { "America/Chicago", I18N_NOOP2("Timezone comment", "Central (most areas)") },
    { "America/Chihuahua", I18N_NOOP2("Timezone comment", "Chihuahua (most areas)") },
    { "America/Ciudad_Juarez", I18N_NOOP2("Timezone comment", "Chihuahua (US border - west)") },
    { "America/Creston", I18N_NOOP2("Timezone comment", "MST - BC (Creston)") },
    { "America/Cuiaba", I18N_NOOP2("Timezone comment", "Mato Grosso") },
    { "America/Danmarkshavn", I18N_NOOP2("Timezone comment", "National Park (east coast)") },
    { "America/Dawson", I18N_NOOP2("Timezone comment", "MST - Yukon (west)") },
    { "America/Dawson_Creek", I18N_NOOP2("Timezone comment", "MST - BC (Dawson Cr, Ft St John)") },
    { "America/Denver", I18N_NOOP2("Timezone comment", "Mountain (most areas)") },
    { "America/Detroit", I18N_NOOP2("Timezone comment", "Eastern - MI (most areas)") },
    { "America/Edmonton", I18N_NOOP2("Timezone comment", "Mountain - AB, BC(E), NT(E), SK(W)") },
    { "America/Eirunepe", I18N_NOOP2("Timezone comment", "Amazonas (west)") },
    { "America/Fort_Nelson", I18N_NOOP2("Timezone comment", "MST - BC (Ft Nelson)") },
    { "America/Fortaleza", I18N_NOOP2("Timezone comment", "Brazil (northeast: MA, PI, CE, RN, PB)") },
    { "America/Glace_Bay", I18N_NOOP2("Timezone comment", "Atlantic - NS (Cape Breton)") },
    { "America/Goose_Bay", I18N_NOOP2("Timezone comment", "Atlantic - Labrador (most areas)") },
    { "America/Guayaquil", I18N_NOOP2("Timezone comment", "Ecuador (mainland)") },
    { "America/Halifax", I18N_NOOP2("Timezone comment", "Atlantic - NS (most areas), PE") },
    { "America/Hermosillo", I18N_NOOP2("Timezone comment", "Sonora") },
    { "America/Indiana/Indianapolis", I18N_NOOP2("Timezone comment", "Eastern - IN (most areas)") },
    { "America/Indiana/Knox", I18N_NOOP2("Timezone comment", "Central - IN (Starke)") },
    { "America/Indiana/Marengo", I18N_NOOP2("Timezone comment", "Eastern - IN (Crawford)") },
    { "America/Indiana/Petersburg", I18N_NOOP2("Timezone comment", "Eastern - IN (Pike)") },
    { "America/Indiana/Tell_City", I18N_NOOP2("Timezone comment", "Central - IN (Perry)") },
    { "America/Indiana/Vevay", I18N_NOOP2("Timezone comment", "Eastern - IN (Switzerland)") },
    { "America/Indiana/Vincennes", I18N_NOOP2("Timezone comment", "Eastern - IN (Da, Du, K, Mn)") },
    { "America/Indiana/Winamac", I18N_NOOP2("Timezone comment", "Eastern - IN (Pulaski)") },
    { "America/Inuvik", I18N_NOOP2("Timezone comment", "Mountain - NT (west)") },
    { "America/Iqaluit", I18N_NOOP2("Timezone comment", "Eastern - NU (most areas)") },
    { "America/Juneau", I18N_NOOP2("Timezone comment", "Alaska - Juneau area") },
    { "America/Kentucky/Louisville", I18N_NOOP2("Timezone comment", "Eastern - KY (Louisville area)") },
    { "America/Kentucky/Monticello", I18N_NOOP2("Timezone comment", "Eastern - KY (Wayne)") },
    { "America/Los_Angeles", I18N_NOOP2("Timezone comment", "Pacific") },
    { "America/Maceio", I18N_NOOP2("Timezone comment", "Alagoas, Sergipe") },
    { "America/Manaus", I18N_NOOP2("Timezone comment", "Amazonas (east)") },
    { "America/Matamoros", I18N_NOOP2("Timezone comment", "Coahuila, Nuevo Leon, Tamaulipas (US border)") },
    { "America/Mazatlan", I18N_NOOP2("Timezone comment", "Baja California Sur, Nayarit (most areas), Sinaloa") },
    { "America/Menominee", I18N_NOOP2("Timezone comment", "Central - MI (Wisconsin border)") },
    { "America/Merida", I18N_NOOP2("Timezone comment", "Campeche, Yucatan") },
    { "America/Metlakatla", I18N_NOOP2("Timezone comment", "Alaska - Annette Island") },
    { "America/Mexico_City", I18N_NOOP2("Timezone comment", "Central Mexico") },
    { "America/Moncton", I18N_NOOP2("Timezone comment", "Atlantic - New Brunswick") },
    { "America/Monterrey", I18N_NOOP2("Timezone comment", "Durango; Coahuila, Nuevo Leon, Tamaulipas (most areas)") },
    { "America/New_York", I18N_NOOP2("Timezone comment", "Eastern (most areas)") },
    { "America/Nome", I18N_NOOP2("Timezone comment", "Alaska (west)") },
    { "America/Noronha", I18N_NOOP2("Timezone comment", "Atlantic islands") },
    { "America/North_Dakota/Beulah", I18N_NOOP2("Timezone comment", "Central - ND (Mercer)") },
    { "America/North_Dakota/Center", I18N_NOOP2("Timezone comment", "Central - ND (Oliver)") },
    { "America/North_Dakota/New_Salem", I18N_NOOP2("Timezone comment", "Central - ND (Morton rural)") },
    { "America/Nuuk", I18N_NOOP2("Timezone comment", "most of Greenland") },
    { "America/Ojinaga", I18N_NOOP2("Timezone comment", "Chihuahua (US border - east)") },
    { "America/Phoenix", I18N_NOOP2("Timezone comment", "MST - AZ (except Navajo)") },
    { "America/Porto_Velho", I18N_NOOP2("Timezone comment", "Rondonia") },
    { "America/Punta_Arenas", I18N_NOOP2("Timezone comment", "Region of Magallanes") },
    { "America/Rankin_Inlet", I18N_NOOP2("Timezone comment", "Central - NU (central)") },
    { "America/Recife", I18N_NOOP2("Timezone comment", "Pernambuco") },
    { "America/Regina", I18N_NOOP2("Timezone comment", "CST - SK (most areas)") },
    { "America/Resolute", I18N_NOOP2("Timezone comment", "Central - NU (Resolute)") },
    { "America/Rio_Branco", I18N_NOOP2("Timezone comment", "Acre") },
    { "America/Santarem", I18N_NOOP2("Timezone comment", "Para (west)") },
    { "America/Santiago", I18N_NOOP2("Timezone comment", "most of Chile") },
    { "America/Sao_Paulo", I18N_NOOP2("Timezone comment", "Brazil (southeast: GO, DF, MG, ES, RJ, SP, PR, SC, RS)") },
    { "America/Scoresbysund", I18N_NOOP2("Timezone comment", "Scoresbysund/Ittoqqortoormiit") },
    { "America/Sitka", I18N_NOOP2("Timezone comment", "Alaska - Sitka area") },
    { "America/St_Johns", I18N_NOOP2("Timezone comment", "Newfoundland, Labrador (SE)") },
    { "America/Swift_Current", I18N_NOOP2("Timezone comment", "CST - SK (midwest)") },
    { "America/Thule", I18N_NOOP2("Timezone comment", "Thule/Pituffik") },
    { "America/Tijuana", I18N_NOOP2("Timezone comment", "Baja California") },
    { "America/Toronto", I18N_NOOP2("Timezone comment", "Eastern - ON & QC (most areas)") },
    { "America/Vancouver", I18N_NOOP2("Timezone comment", "Pacific - BC (most areas)") },
    { "America/Whitehorse", I18N_NOOP2("Timezone comment", "MST - Yukon (east)") },
    { "America/Winnipeg", I18N_NOOP2("Timezone comment", "Central - ON (west), Manitoba") },
    { "America/Yakutat", I18N_NOOP2("Timezone comment", "Alaska - Yakutat") },
    { "Antarctica/Casey", I18N_NOOP2("Timezone comment", "Casey") },
    { "Antarctica/Davis", I18N_NOOP2("Timezone comment", "Davis") },
    { "Antarctica/DumontDUrville", I18N_NOOP2("Timezone comment", "Dumont-d'Urville") },
    { "Antarctica/Macquarie", I18N_NOOP2("Timezone comment", "Macquarie Island") },
    { "Antarctica/Mawson", I18N_NOOP2("Timezone comment", "Mawson") },
    { "Antarctica/McMurdo", I18N_NOOP2("Timezone comment", "New Zealand time - McMurdo, South Pole") },
    { "Antarctica/Palmer", I18N_NOOP2("Timezone comment", "Palmer") },
    { "Antarctica/Rothera", I18N_NOOP2("Timezone comment", "Rothera") },
    { "Antarctica/Syowa", I18N_NOOP2("Timezone comment", "Syowa") },
    { "Antarctica/Troll", I18N_NOOP2("Timezone comment", "Troll") },
    { "Antarctica/Vostok", I18N_NOOP2("Timezone comment", "Vostok") },
    { "Asia/Almaty", I18N_NOOP2("Timezone comment", "most of Kazakhstan") },
    { "Asia/Anadyr", I18N_NOOP2("Timezone comment", "MSK+09 - Bering Sea") },
    { "Asia/Aqtau", I18N_NOOP2("Timezone comment", "Mangghystau/Mankistau") },
    { "Asia/Aqtobe", I18N_NOOP2("Timezone comment", "Aqtobe/Aktobe") },
    { "Asia/Atyrau", I18N_NOOP2("Timezone comment", "Atyrau/Atirau/Gur'yev") },
    { "Asia/Barnaul", I18N_NOOP2("Timezone comment", "MSK+04 - Altai") },
    { "Asia/Chita", I18N_NOOP2("Timezone comment", "MSK+06 - Zabaykalsky") },
    { "Asia/Choibalsan", I18N_NOOP2("Timezone comment", "Dornod, Sukhbaatar") },
    { "Asia/Famagusta", I18N_NOOP2("Timezone comment", "Northern Cyprus") },
    { "Asia/Gaza", I18N_NOOP2("Timezone comment", "Gaza Strip") },
    { "Asia/Hebron", I18N_NOOP2("Timezone comment", "West Bank") },
    { "Asia/Hovd", I18N_NOOP2("Timezone comment", "Bayan-Olgiy, Govi-Altai, Hovd, Uvs, Zavkhan") },
    { "Asia/Irkutsk", I18N_NOOP2("Timezone comment", "MSK+05 - Irkutsk, Buryatia") },
    { "Asia/Jakarta", I18N_NOOP2("Timezone comment", "Java, Sumatra") },
    { "Asia/Jayapura", I18N_NOOP2("Timezone comment", "New Guinea (West Papua / Irian Jaya), Malukus/Moluccas") },
    { "Asia/Kamchatka", I18N_NOOP2("Timezone comment", "MSK+09 - Kamchatka") },
    { "Asia/Khandyga", I18N_NOOP2("Timezone comment", "MSK+06 - Tomponsky, Ust-Maysky") },
    { "Asia/Krasnoyarsk", I18N_NOOP2("Timezone comment", "MSK+04 - Krasnoyarsk area") },
    { "Asia/Kuala_Lumpur", I18N_NOOP2("Timezone comment", "Malaysia (peninsula)") },
    { "Asia/Kuching", I18N_NOOP2("Timezone comment", "Sabah, Sarawak") },
    { "Asia/Magadan", I18N_NOOP2("Timezone comment", "MSK+08 - Magadan") },
    { "Asia/Makassar", I18N_NOOP2("Timezone comment", "Borneo (east, south), Sulawesi/Celebes, Bali, Nusa Tengarra, Timor (west)") },
    { "Asia/Nicosia", I18N_NOOP2("Timezone comment", "most of Cyprus") },
    { "Asia/Novokuznetsk", I18N_NOOP2("Timezone comment", "MSK+04 - Kemerovo") },
    { "Asia/Novosibirsk", I18N_NOOP2("Timezone comment", "MSK+04 - Novosibirsk") },
    { "Asia/Omsk", I18N_NOOP2("Timezone comment", "MSK+03 - Omsk") },
    { "Asia/Oral", I18N_NOOP2("Timezone comment", "West Kazakhstan") },
    { "Asia/Pontianak", I18N_NOOP2("Timezone comment", "Borneo (west, central)") },
    { "Asia/Qostanay", I18N_NOOP2("Timezone comment", "Qostanay/Kostanay/Kustanay") },
    { "Asia/Qyzylorda", I18N_NOOP2("Timezone comment", "Qyzylorda/Kyzylorda/Kzyl-Orda") },
    { "Asia/Sakhalin", I18N_NOOP2("Timezone comment", "MSK+08 - Sakhalin Island") },
    { "Asia/Samarkand", I18N_NOOP2("Timezone comment", "Uzbekistan (west)") },
    { "Asia/Shanghai", I18N_NOOP2("Timezone comment", "Beijing Time") },
    { "Asia/Srednekolymsk", I18N_NOOP2("Timezone comment", "MSK+08 - Sakha (E), N Kuril Is") },
    { "Asia/Tashkent", I18N_NOOP2("Timezone comment", "Uzbekistan (east)") },
    { "Asia/Tomsk", I18N_NOOP2("Timezone comment", "MSK+04 - Tomsk") },
    { "Asia/Ulaanbaatar", I18N_NOOP2("Timezone comment", "most of Mongolia") },
    { "Asia/Urumqi", I18N_NOOP2("Timezone comment", "Xinjiang Time") },
    { "Asia/Ust-Nera", I18N_NOOP2("Timezone comment", "MSK+07 - Oymyakonsky") },
    { "Asia/Vladivostok", I18N_NOOP2("Timezone comment", "MSK+07 - Amur River") },
    { "Asia/Yakutsk", I18N_NOOP2("Timezone comment", "MSK+06 - Lena River") },
    { "Asia/Yekaterinburg", I18N_NOOP2("Timezone comment", "MSK+02 - Urals") },
    { "Atlantic/Azores", I18N_NOOP2("Timezone comment", "Azores") },
    { "Atlantic/Canary", I18N_NOOP2("Timezone comment", "Canary Islands") },
    { "Atlantic/Madeira", I18N_NOOP2("Timezone comment", "Madeira Islands") },
    { "Australia/Adelaide", I18N_NOOP2("Timezone comment", "South Australia") },
    { "Australia/Brisbane", I18N_NOOP2("Timezone comment", "Queensland (most areas)") },
    { "Australia/Broken_Hill", I18N_NOOP2("Timezone comment", "New South Wales (Yancowinna)") },
    { "Australia/Darwin", I18N_NOOP2("Timezone comment", "Northern Territory") },
    { "Australia/Eucla", I18N_NOOP2("Timezone comment", "Western Australia (Eucla)") },
    { "Australia/Hobart", I18N_NOOP2("Timezone comment", "Tasmania") },
    { "Australia/Lindeman", I18N_NOOP2("Timezone comment", "Queensland (Whitsunday Islands)") },
    { "Australia/Lord_Howe", I18N_NOOP2("Timezone comment", "Lord Howe Island") },
    { "Australia/Melbourne", I18N_NOOP2("Timezone comment", "Victoria") },
    { "Australia/Perth", I18N_NOOP2("Timezone comment", "Western Australia (most areas)") },
    { "Australia/Sydney", I18N_NOOP2("Timezone comment", "New South Wales (most areas)") },
    { "Europe/Astrakhan", I18N_NOOP2("Timezone comment", "MSK+01 - Astrakhan") },
    { "Europe/Berlin", I18N_NOOP2("Timezone comment", "most of Germany") },
    { "Europe/Busingen", I18N_NOOP2("Timezone comment", "Busingen") },
    { "Europe/Kaliningrad", I18N_NOOP2("Timezone comment", "MSK-01 - Kaliningrad") },
    { "Europe/Kirov", I18N_NOOP2("Timezone comment", "MSK+00 - Kirov") },
    { "Europe/Kyiv", I18N_NOOP2("Timezone comment", "most of Ukraine") },
    { "Europe/Lisbon", I18N_NOOP2("Timezone comment", "Portugal (mainland)") },
    { "Europe/Madrid", I18N_NOOP2("Timezone comment", "Spain (mainland)") },
    { "Europe/Moscow", I18N_NOOP2("Timezone comment", "MSK+00 - Moscow area") },
    { "Europe/Samara", I18N_NOOP2("Timezone comment", "MSK+01 - Samara, Udmurtia") },
    { "Europe/Saratov", I18N_NOOP2("Timezone comment", "MSK+01 - Saratov") },
    { "Europe/Simferopol", I18N_NOOP2("Timezone comment", "Crimea") },
    { "Europe/Ulyanovsk", I18N_NOOP2("Timezone comment", "MSK+01 - Ulyanovsk") },
    { "Europe/Volgograd", I18N_NOOP2("Timezone comment", "MSK+00 - Volgograd") },
    { "Pacific/Auckland", I18N_NOOP2("Timezone comment", "most of New Zealand") },
    { "Pacific/Bougainville", I18N_NOOP2("Timezone comment", "Bougainville") },
    { "Pacific/Chatham", I18N_NOOP2("Timezone comment", "Chatham Islands") },
    { "Pacific/Chuuk", I18N_NOOP2("Timezone comment", "Chuuk/Truk, Yap") },
    { "Pacific/Easter", I18N_NOOP2("Timezone comment", "Easter Island") },
    { "Pacific/Galapagos", I18N_NOOP2("Timezone comment", "Galapagos Islands") },
    { "Pacific/Gambier", I18N_NOOP2("Timezone comment", "Gambier Islands") },
    { "Pacific/Honolulu", I18N_NOOP2("Timezone comment", "Hawaii") },
    { "Pacific/Kanton", I18N_NOOP2("Timezone comment", "Phoenix Islands") },
    { "Pacific/Kiritimati", I18N_NOOP2("Timezone comment", "Line Islands") },
    { "Pacific/Kosrae", I18N_NOOP2("Timezone comment", "Kosrae") },
    { "Pacific/Kwajalein", I18N_NOOP2("Timezone comment", "Kwajalein") },
    { "Pacific/Majuro", I18N_NOOP2("Timezone comment", "most of Marshall Islands") },
    { "Pacific/Marquesas", I18N_NOOP2("Timezone comment", "Marquesas Islands") },
    { "Pacific/Midway", I18N_NOOP2("Timezone comment", "Midway Islands") },
    { "Pacific/Pohnpei", I18N_NOOP2("Timezone comment", "Pohnpei/Ponape") },
    { "Pacific/Port_Moresby", I18N_NOOP2("Timezone comment", "most of Papua New Guinea") },
    { "Pacific/Tahiti", I18N_NOOP2("Timezone comment", "Society Islands") },
    { "Pacific/Tarawa", I18N_NOOP2("Timezone comment", "Gilbert Islands") },
    { "Pacific/Wake", I18N_NOOP2("Timezone comment", "Wake Island") },
};
static const qint16 timezoneCommentTblSize = sizeof(timezoneCommentTbl) / sizeof(timezoneCommentData);

QString zoneinfoDir()
{
    static const QStringList zoneinfodirs = QStringList()
        << QString::fromLatin1("/share/zoneinfo")
        << QString::fromLatin1("/lib/zoneinfo")
        << QString::fromLatin1("/usr/share/zoneinfo")
        << QString::fromLatin1("/usr/lib/zoneinfo")
        << QString::fromLatin1("/usr/local/share/zoneinfo")
        << QString::fromLatin1("/usr/local/lib/zoneinfo")
        << (QString::fromLatin1(KDEDIR) + QLatin1String("/share/zoneinfo"))
        << (QString::fromLatin1(KDEDIR) + QLatin1String("/lib/zoneinfo"));
    foreach (const QString &zoneinfodir, zoneinfodirs) {
        KDE_struct_stat statbuf;
        if (KDE::stat(zoneinfodir, &statbuf) == 0 && S_ISDIR(statbuf.st_mode)) {
            return zoneinfodir;
        }
    }
    return zoneinfodirs.last();
}

// Convert sHHMM or sHHMMSS to a floating point number of degrees.
static float convertCoordinate(const QByteArray &coordinate)
{
    int value = coordinate.toInt();
    int degrees = 0;
    int minutes = 0;
    int seconds = 0;

    if (coordinate.length() > 6) {
        degrees = value / 10000;
        value -= degrees * 10000;
        minutes = value / 100;
        value -= minutes * 100;
        seconds = value;
    } else {
        degrees = value / 100;
        value -= degrees * 100;
        minutes = value;
    }
    value = degrees * 3600 + minutes * 60 + seconds;
    return value / 3600.0;
}

static KZoneCoordinates splitZoneTabCoordinates(const QByteArray &zonetabcoordinates)
{
    KZoneCoordinates result = qMakePair(KTimeZone::UNKNOWN, KTimeZone::UNKNOWN);
    int startindex = 0;
    if (zonetabcoordinates.startsWith('+') || zonetabcoordinates.startsWith('-')) {
        startindex = 1;
    }
    int signindex = zonetabcoordinates.indexOf('+', startindex);
    if (signindex < startindex) {
        signindex = zonetabcoordinates.indexOf('-', startindex);
    }
    if (signindex < startindex) {
        return result;
    }
    const float latitude = convertCoordinate(zonetabcoordinates.mid(0, signindex));
    const float longitude = convertCoordinate(zonetabcoordinates.mid(signindex, zonetabcoordinates.size() - signindex));
    result = qMakePair(latitude, longitude);
    return result;
}

class KSystemTimeZonesPrivate : public QObject
{
    Q_OBJECT
public:
    KSystemTimeZonesPrivate();

    KTimeZone findZone(const QString &name) const;

    KTimeZoneList m_zones;
    KTimeZone m_localtz;
    QString m_zoneinfoDir;

private Q_SLOTS:
    void update(const QString &path);

private:
    QFileSystemWatcher *m_watcher;
};

KSystemTimeZonesPrivate::KSystemTimeZonesPrivate()
    : m_watcher(nullptr)
{
    update(QString());
}

KTimeZone KSystemTimeZonesPrivate::findZone(const QString &name) const
{
    foreach (const KTimeZone &zone, m_zones) {
        if (zone.name() == name) {
            return zone;
        }
    }
    return KTimeZone();
}

void KSystemTimeZonesPrivate::update(const QString &path)
{
    Q_UNUSED(path);

#ifndef NDEBUG
    QElapsedTimer updatetimer;
    updatetimer.start();
#endif

    m_localtz = KTimeZone::utc();
    static const QByteArray kdezoneinfodir = qgetenv("KDE_ZONEINFO_DIR");
    m_zoneinfoDir = QFile::decodeName(kdezoneinfodir);
    if (m_zoneinfoDir.isEmpty()) {
        m_zoneinfoDir = zoneinfoDir();
    }

    m_zones.clear();
    // UTC is used as fallback, should be there in any case
    m_zones.append(m_localtz);

    const QString zonetab = m_zoneinfoDir + QLatin1String("/zone.tab");
    QFile zonetabfile(zonetab);
    if (!zonetabfile.open(QFile::ReadOnly)) {
        kWarning(161) << "Could not open zone.tab" << zonetab;
        return;
    }

    QStringList watchlist = QStringList()
        << zonetab
        << s_localtime;
    QString reallocaltime(s_localtime);
    QFileInfo localtimeinfo(s_localtime);
    if (localtimeinfo.isSymLink()) {
        reallocaltime = localtimeinfo.readLink();
        watchlist.append(reallocaltime);
    }
    if (m_watcher) {
        m_watcher->deleteLater();
    }
    m_watcher = new QFileSystemWatcher(this);
    m_watcher->addPaths(watchlist);
    connect(m_watcher, SIGNAL(fileChanged(QString)), this, SLOT(update(QString)));

    kDebug(161) << "Parsing" << zonetab;
    char zonecode[4];
    char zonecoordinates[32];
    char zonename[128];
    char zonecomment[1024];
    while (!zonetabfile.atEnd()) {
        const QByteArray zonetabline = zonetabfile.readLine().trimmed();
        if (zonetabline.isEmpty() || zonetabline.startsWith('#')) {
            continue;
        }

        ::memset(zonecode, '\0', sizeof(zonecode));
        ::memset(zonecoordinates, '\0', sizeof(zonecode));
        ::memset(zonename, '\0', sizeof(zonename));
        ::memset(zonecomment, '\0', sizeof(zonecomment));
        const int sscanfresult = sscanf(
            zonetabline.constData(),
            "%3s %31s %127s %1023[^\n]",
            zonecode, zonecoordinates, zonename, zonecomment
        );

        if (Q_UNLIKELY(sscanfresult < 3 || sscanfresult > 4)) {
            kWarning(161) << "Invalid zone.tab entry" << zonetabline;
            continue;
        }

        const KZoneCoordinates zonetabcoordinates = splitZoneTabCoordinates(
            QByteArray::fromRawData(zonecoordinates, qstrlen(zonecoordinates))
        );
        if (Q_UNLIKELY(zonetabcoordinates.first == KTimeZone::UNKNOWN
            || zonetabcoordinates.second == KTimeZone::UNKNOWN)) {
            kWarning(161) << "Invalid zone.tab coordinates" << zonetabline;
            continue;
        }

        const KTimeZone ktimezone(
            QString::fromLatin1(zonename),
            QString::fromLatin1(zonecode),
            zonetabcoordinates.first, zonetabcoordinates.second,
            QString::fromLatin1(zonecomment)
        );
        m_zones.append(ktimezone);
    }

    if (localtimeinfo.isSymLink()) {
        const int zonediroffset = (m_zoneinfoDir.size() + 1);
        const QString localtz = reallocaltime.mid(zonediroffset, reallocaltime.size() - zonediroffset);
        m_localtz = findZone(localtz);
#ifndef NDEBUG
        kDebug(161) << "Zones update took" << updatetimer.elapsed() << "ms";
#endif
        return;
    }

    time_t ltime;
    ::time(&ltime);
    ::tzset();
    struct tm res;
    struct tm *t = ::localtime_r(&ltime, &res);
#if defined(HAVE_STRUCT_TM_TM_ZONE)
    const QByteArray localtz(t->tm_zone);
#else
    const QByteArray localtz(tzname[t->tm_isdst]);
#endif
    foreach (const KTimeZone &zone, m_zones) {
        if (zone.abbreviations().contains(localtz)) {
            m_localtz = zone;
#ifndef NDEBUG
            kDebug(161) << "Zones update took" << updatetimer.elapsed() << "ms";
#endif
            break;
        }
    }
}

K_GLOBAL_STATIC(KSystemTimeZonesPrivate, s_systemzones);

/******************************************************************************/

KTimeZone KSystemTimeZones::local()
{
    const QByteArray envtz = qgetenv("TZ");
    if (!envtz.isEmpty()) {
        if (envtz.at(0) == ':') {
            return KSystemTimeZones::zone(QString::fromLocal8Bit(envtz.constData() + 1, envtz.size() - 1));
        }
        return KSystemTimeZones::zone(QString::fromLocal8Bit(envtz.constData(), envtz.size()));
    }

    return s_systemzones->m_localtz;
}

QString KSystemTimeZones::zoneinfoDir()
{
    return s_systemzones->m_zoneinfoDir;
}

const KTimeZoneList KSystemTimeZones::zones()
{
    return s_systemzones->m_zones;
}

KTimeZone KSystemTimeZones::zone(const QString &name)
{
    return s_systemzones->findZone(name);
}

QString KSystemTimeZones::zoneName(const QString &name)
{
    if (name.isEmpty()) {
        kWarning(161) << "Zone name is empty";
        return QString();
    }
    const QByteArray namelatin1 = name.toLatin1();
    for (int i = 0; i < timezoneNameTblSize; i++) {
        if (qstrcmp(namelatin1.constData(), timezoneNameTbl[i].name) == 0) {
            // for better or worse replacing underscore with space was done before and is still done for compat
            return ki18nc("Timezone name", timezoneNameTbl[i].translation).toString().replace(QLatin1Char('_'), QLatin1Char(' '));
        }
    }
    return name;
}

QString KSystemTimeZones::zoneComment(const QString &name)
{
    if (name.isEmpty()) {
        kWarning(161) << "Zone name is empty";
        return QString();
    }
    const QByteArray namelatin1 = name.toLatin1();
    for (int i = 0; i < timezoneCommentTblSize; i++) {
        if (qstrcmp(namelatin1.constData(), timezoneCommentTbl[i].name) == 0) {
            return ki18nc("Timezone comment", timezoneCommentTbl[i].translation).toString();
        }
    }
    return QString();
}

#include "ksystemtimezone.moc"
