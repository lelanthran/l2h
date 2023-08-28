#!/bin/bash
die() {
   echo $@
   exit 127
}

[ -z "$1" ] && die "Need count of first-level directories."
[ -z "$2" ] && die "Need count of second-level directories."

rm -rf speed-test tmp.txt dcreate.txt dconvert.txt

create_dirs() {
   echo "[$1:$2]"
   for ((X=0;X<$1;X++)); do
      for ((Y=0;Y<$2;Y++)); do
         mkdir -p speed-test/$X/$Y; cp speed-test.html.lisp speed-test/$X/$Y;
      done;
   done;
   echo created dirs
}

export DURATION_CREATE="$(time -p (create_dirs $1 $2) &> dcreate.txt)"
export INDIVIDUAL_FSIZE=`du -msh speed-test.html.lisp`
export TOTAL_FSIZE=$(cat `find speed-test -type f` > tmp.txt && du -msh tmp.txt)
export DIRCOUNT=`find speed-test -type d | wc -l`

export DURATION_CONVERT=`time -p (./l2h -r speed-test) &> dconvert.txt`

echo FILECOUNT,$(($1 * $2))
echo INDIVIDUAL_FSIZE,`echo $INDIVIDUAL_FSIZE | cut -f 1 -d \  `
echo TOTAL_FSIZE,`echo $TOTAL_FSIZE | cut -f 1 -d \  `
echo DIRCOUNT,$DIRCOUNT
echo DURATION_CREATE,`grep real dcreate.txt | cut -f 2  -d \ `
echo DURATION_CONVERT,`grep real dconvert.txt | cut -f 2 -d \  `



