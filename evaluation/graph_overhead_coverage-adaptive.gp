set terminal postscript enhanced color solid
set size 0.8,0.8
set output "graph_overhead_coverage-adaptive.eps"
set title "Adaptive protocols comparison: coverage"
set xlabel "Overhead ratio ({/Symbol r})"
set ylabel "Coverage (%)"
set key right bottom
set yrange [60:103]
set xrange [1:3.3]

plot \
"adaptive_specific.dat"	using 5:2       title "adaptive dissemination, alg. #3" with points pointtype 3 pointsize 0.5 lc 4, \
"adaptive.dat"		using 5:2       title "adaptive dissemination, alg. #1" with points pointtype 4 pointsize 0.5 lc 3, \
"adaptive_sender.dat"	using 5:2      	title "adaptive dissemination, alg. #2" with points pointtype 2 pointsize 0.5 lc 5

show key
show xlabel
show ylabel
show output
