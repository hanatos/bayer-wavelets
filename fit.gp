set term pdf
set print 'fit.log'
set output 'plot.pdf'
set fit logfile "/dev/null"
set title "Histogram"
plot "noise.dat" u 1:(log($5)) w l lw 4 title "red",    \
  '' u 1:(log($6)) w l lw 4 title "green",     \
  '' u 1:(log($7)) w l lw 4 title "blue"

min(x,y) = (x < y) ? x : y
max(x,y) = (x > y) ? x : y
f1(x) = a1*x + b1
f2(x) = a2*x + b2
f3(x) = a3*x + b3
a1=0.1;b1=0.001;
a2=0.1;b2=0.001;
a3=0.1;b3=0.001;
set xrange [0.0:0.25]
fit f1(x) "noise.dat" u 1:($2**2):(1/max(0.001, $5)) via a1,b1
set xrange [0.0:0.25]
fit f2(x) "noise.dat" u 1:($3**2):(1/max(0.001, $6)) via a2,b2
set xrange [0.0:0.25]
fit f3(x) "noise.dat" u 1:($4**2):(1/max(0.001, $7)) via a3,b3

set xrange [0:0.5]
set title "Noise levels"
plot "noise.dat" u 1:2 w l lw 4 title "red",       \
  '' u 1:3 w l lw 4 title "green",        \
  '' u 1:4 w l lw 4 title" blue",       \
  '' u 1:(sqrt(f1($1))) w l lw 2 lt 1 title "red (fit)", \
  '' u 1:(sqrt(f2($1))) w l lw 2 lt 2 title "green (fit)", \
  '' u 1:(sqrt(f3($1))) w l lw 2 lt 3 title "blue (fit)"

print a1, a2, a3, b1, b2, b3
