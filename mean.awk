{
   if (NR == 1) {
       sum=min=max=$1;
   } else {
       sum += $1;
   }
}
END {
   printf "%f\n", sum/NR;
}
