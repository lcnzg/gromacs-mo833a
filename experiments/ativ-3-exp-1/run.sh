#!/usr/bin/env bash

BUILDS=(release debug)
N=20

./compile.sh

./download.sh

if [[ ! -f "em.tpr" ]]; then
    GMX="../../build-$BUILDS/bin/gmx"
    $GMX pdb2gmx -f 6LVN.pdb -o 6LVN_processed.gro -water spce -ff oplsaa
    $GMX editconf -f 6LVN_processed.gro -o 6LVN_newbox.gro -c -d 1.0 -bt cubic
    $GMX solvate -cp 6LVN_newbox.gro -cs spc216.gro -o 6LVN_solv.gro -p topol.top
    $GMX grompp -f ions.mdp -c 6LVN_solv.gro -p topol.top -o ions.tpr
    echo "SOL" | $GMX genion -s ions.tpr -o 6LVN_solv_ions.gro -p topol.top -pname NA -nname CL -neutral
    $GMX grompp -f ions.mdp -c 6LVN_solv_ions.gro -p topol.top -o em.tpr
fi

for BUILD in "${BUILDS[@]}"
do
    GMX="../../build-$BUILD/bin/gmx"
    OUT="out-$BUILD.csv"
    : > $OUT
    for ((i=0; i < N; i++))
    do
        $GMX mdrun -v -deffnm em | grep "[MO833]" | cut -d' ' -f5 >> $OUT
    done
done
