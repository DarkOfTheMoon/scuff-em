#!/bin/bash

#################################################################
#################################################################
#################################################################
if [ "x$1" == "x--debug" ]
then
  DIR=${SEDB}
  PREFIX="gdb --args"
  shift
elif [ "x$1" == "x--valgrind" ]
then
  DIR=${SEDB}
  PREFIX="valgrind"
  shift
else
  DIR=${SEI}
  PREFIX=""
fi

#################################################################
#################################################################
#################################################################
DIR=${SEI}
CODE=scuff-static

GEOM=StriplineCapacitor

#################################################################
#################################################################
#################################################################
N=4 # meshing fineness
/bin/rm ${GEOM}.CapVsT
for T in 0.5 0.6 0.7 0.8 0.9 1.0 1.1 1.2 1.3 1.4 1.5 1.6 1.7 1.8 1.9 2.0
do

  #################################################################
  #################################################################
  #################################################################
  gmsh -2 -setnumber N ${N} -setnumber T ${T} ${GEOM}.geo
  NUMEDGES=`scuff-analyze --mesh ${GEOM}.msh  | grep 'interior edges' | head -1 | cut -f2 -d' '`

  #################################################################
  #################################################################
  #################################################################
  echo "T=${T}: NumEdges=${NUMEDGES}"
  #/bin/rm "${GEOM}.CapMatrix"

  ARGS=""
  ARGS="${ARGS} --geometry ${GEOM}.scuffgeo"
  ARGS="${ARGS} --CapFile  ${GEOM}.CapMatrix"
  ${PREFIX} ${DIR}/${CODE} ${ARGS}

  LINE=`tail -1 ${GEOM}.CapMatrix`
  echo "${T} ${LINE}" >> ${GEOM}.CapVsT

done
