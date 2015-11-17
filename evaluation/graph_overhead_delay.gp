set terminal postscript enhanced color solid
set size 0.8,0.8
set output "graph_overhead_delay.eps"
set title "Dissemination protocol comparison: delay"
set xlabel "Overhead ratio ({/Symbol r})"
set ylabel "Delay (number of hops)"
set key right bottom

plot \
"fixed.dat"             using 5:3       title "fixed probability" with points pointtype 2 pointsize 0.5 lc 2, \
"broadcast.dat"         using 5:3       title "probabilistic broadcast" with points pointtype 3 pointsize 0.5 lc 1

show key
show xlabel
show ylabel
show output
