#!/bin/sh
for n in `find . -name "*.[chly]"`
do
sed '
{
s/NOPASTEFLAG/PCB_FLAG_NOPASTE/g
s/PININPOLYFLAG/PCB_FLAG_PININPOLY/g
s/CLEARPOLYFLAG/PCB_FLAG_CLEARPOLY/g
s/HIDENAMEFLAG/PCB_FLAG_HIDENAME/g
s/DISPLAYNAMEFLAG/PCB_FLAG_DISPLAYNAME/g
s/CLEARLINEFLAG/PCB_FLAG_CLEARLINE/g
s/FULLPOLYFLAG/PCB_FLAG_FULLPOLY/g
s/SELECTEDFLAG/PCB_FLAG_SELECTED/g
s/ONSOLDERFLAG/PCB_FLAG_ONSOLDER/g
s/SQUAREFLAG/PCB_FLAG_SQUARE/g
s/RUBBERENDFLAG/PCB_FLAG_RUBBEREND/g
s/USETHERMALFLAG/PCB_FLAG_USETHERMAL/g
s/ONSILKFLAG/PCB_FLAG_ONSILK/g
s/OCTAGONFLAG/PCB_FLAG_OCTAGON/g
s/LOCKFLAG/PCB_FLAG_LOCK/g
s/EDGE2FLAG/PCB_FLAG_EDGE2/g
s/VISITFLAG/PCB_FLAG_VISIT/g
s/NONETLISTFLAG/PCB_FLAG_NONETLIST/g
s/MINCUTFLAG/PCB_FLAG_MINCUT/g
s/ONPOINTFLAG/PCB_FLAG_ONPOINT/g
s/NOCOPY_FLAGS/PCB_FLAG_NOCOPY/g
s/FOUNDFLAG/PCB_FLAG_FOUND/g
s/HOLEFLAG/PCB_FLAG_HOLE/g
s/AUTOFLAG/PCB_FLAG_AUTO/g
s/WARNFLAG/PCB_FLAG_WARN/g
s/RATFLAG/PCB_FLAG_RAT/g
s/PINFLAG/PCB_FLAG_PIN/g
s/VIAFLAG/PCB_FLAG_VIA/g
s/DRCFLAG/PCB_FLAG_DRC/g
s/NOFLAG/PCB_FLAG_NO/g
}
' -i $n
done
