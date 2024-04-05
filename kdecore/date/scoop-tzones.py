#!/usr/bin/env python

import sys

if len(sys.argv) < 3:
    print('usage: scoop-tzones <name|comment> <zonetab>')
    exit(1)

printname = False
printcomment = False
if sys.argv[1] == 'name':
    printname = True
elif sys.argv[1] == 'comment':
    printcomment = True
else:
    print('usage: scoop-tzones <name|comment> <zonetab>')
    exit(1)

with open(sys.argv[2], 'r') as zonetabhandle:
    for zoneline in zonetabhandle.readlines():
        if printname:
            strippedline = zoneline.strip()
            if strippedline.startswith('#') or len(strippedline) == 0:
                continue
            splitline = strippedline.split('\t')
            splitpart2 = splitline[2].strip()
            print('    { "%s", I18N_NOOP2("Timezone name", "%s") },' % (splitpart2, splitpart2))
        elif printcomment:
            strippedline = zoneline.strip()
            if strippedline.startswith('#') or len(strippedline) == 0:
                continue
            splitline = strippedline.split('\t')
            splitpart2 = splitline[2].strip()
            splitpart3 = ' '.join(splitline[3:]).strip()
            if len(splitpart3) == 0:
                continue
            print('    { "%s", I18N_NOOP2("Timezone comment", "%s") },' % (splitpart2, splitpart3))
