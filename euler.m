(** prob 4. **)
    test = test2[ToString @ #]&;
    test2 = # === StringReverse @ # &;
    acc = Null;
    For[i = 999, i > 0, i--, For[j = i, j > 0, j--, If[test[i j], acc = Cons[{i, j, i j}, acc]]]];
    Max @ Flatten @ acc

(** prob 5. **)
    primes = {2, 3, 5, 7, 11, 13, 17, 19};
    all = Function[{v}, temp = v; r = {}; While[temp > 1, xx = fSelect[primes, Mod[temp, #] === 0&]; temp = Quotient[temp, Times @@ xx]; r = Join[r, xx]]; r];
    factors = Function[{v}, temp = all[v]; Fold[Function[{Acc, X}, Append[Acc, {X, Length @ fSelect[temp, # === X&]}]], {}, primes]];
    Function[{v}, counters[v] = 0] /@ primes;
    includes = Function[{fs}, Function[{F}, counters[F[[1]]] = Max[{counters[F[[1]]], F[[2]]}]] /@ fs];
    Times @@ (# ^ counters[#] & /@ primes)

(** prob. 7 **)
    pQ = Function[{v, n}, If[n <= Length @ allP, Which[Mod[v, allP[[n]]] === 0, False, allP[[n]] ^ 2 > v, True, pQ[v, n + 1]], True]];
    len = 1; allP = {2}; cur = 3;
    While[len < 10001, While[pQ[cur, 1] === False, cur += 2]; allP = Append[allP, cur]; len++; cur += 2; If[Mod[len, 100] == 0, Print @ len]]; cur - 2

(** prob. 8 **)
(* 73167176531330624919225119674426574742355349194934
 96983520312774506326239578318016984801869478851843
 85861560789112949495459501737958331952853208805511
 12540698747158523863050715693290963295227443043557
 66896648950445244523161731856403098711121722383113
 62229893423380308135336276614282806444486645238749
 30358907296290491560440772390713810515859307960866
 70172427121883998797908792274921901699720888093776
 65727333001053367881220235421809751254540594752243
 52584907711670556013604839586446706324415722155397
 53697817977846174064955149290862569321978468622482
 83972241375657056057490261407972968652414535100474
 82166370484403199890008895243450658541227588666881
 16427171479924442928230863465674813919123162824586
 17866458359124566529476545682848912883142607690042
 24219022671055626321111109370544217506941658960408
 07198403850962455444362981230987879927244284909188
 84580156166097919133875499200524063689912560717606
 05886116467109405077541002256983155200055935729725
 71636269561882670428252483600823257530420752963450 *)

    Max[Times @@ (ToExpression /@ {sc[[#]], sc[[#+1]], sc[[#+2]], sc[[#+3]], sc[[#+4]]})& /@ Range[1, 996]]

(** prob. 9**)

    For[i = 1, i < 1000, i++, For[j = i, j < 1000, j++, If[(1000 - i - j) ^ 2 == i ^ 2 + j ^ 2, Print[{i, j, i j (1000 - i - j)}]]]];

(** prob. 10 **)
    bsearch1 = Function[{v, a, b}, n = Quotient[a + b, 2]; temp = allP2[[n]]; Which[a >= b || temp == v, a, temp < v, bsearch1[v, n + 1, b], bsearch1[v, a, n - 1]]];
    bsearch = bsearch1[#, 1, Length @ allP]&;
    pQ = Function[{v}, And @@ (Mod[v, allP[[#]]] =!= 0 & /@ Range @ bsearch @ v)];
    gen6 = Flatten[Fold[Cons[#2 + 1, Cons[#2 - 1, #1]]&, Null, #], Reverse]&;
    allP = {}; allP2 = {}; xx = gen6 @ Range[6, 1506, 6];
    If[pQ[#1], allP = Append[allP, #]; allP2 = Append[allP2, #^2];]& /@ xx;
    rr = Fold[If[pQ[#2], #2 + #1, #1]&, 0, Range[1511, 2000000, 6]];
    Fold[If[pQ[#2], #2 + #1, #1]&, rr, Range[1513, 2000000, 6]];

    MAX = 2000000;
    Function[{n}, pQ[n] = True] /@ Range[2, MAX];
    find = Function[{n}, If[pQ[n] === False, find[n+1], n]];
    sieve = Function[{n, acc}, t = find[n]; For[i = t + t, i < MAX, i += t, pQ[i] = False]; If[t < MAX, sieve[t + 1, acc + t], acc]];
    sieve[2, 0] // Timing
    ==>{48.22,142913828922}

(** prob. 11 **)

ss = {
" 08 02 22 97 38 15 00 40 00 75 04 05 07 78 52 12 50 77 91 08", 
" 49 49 99 40 17 81 18 57 60 87 17 40 98 43 69 48 04 56 62 00", 
" 81 49 31 73 55 79 14 29 93 71 40 67 53 88 30 03 49 13 36 65", 
" 52 70 95 23 04 60 11 42 69 24 68 56 01 32 56 71 37 02 36 91", 
" 22 31 16 71 51 67 63 89 41 92 36 54 22 40 40 28 66 33 13 80", 
" 24 47 32 60 99 03 45 02 44 75 33 53 78 36 84 20 35 17 12 50", 
" 32 98 81 28 64 23 67 10 26 38 40 67 59 54 70 66 18 38 64 70", 
" 67 26 20 68 02 62 12 20 95 63 94 39 63 08 40 91 66 49 94 21", 
" 24 55 58 05 66 73 99 26 97 17 78 78 96 83 14 88 34 89 63 72", 
" 21 36 23 09 75 00 76 44 20 45 35 14 00 61 33 97 34 31 33 95", 
" 78 17 53 28 22 75 31 67 15 94 03 80 04 62 16 14 09 53 56 92", 
" 16 39 05 42 96 35 31 47 55 58 88 24 00 17 54 24 36 29 85 57", 
" 86 56 00 48 35 71 89 07 05 44 44 37 44 60 21 58 51 54 17 58", 
" 19 80 81 68 05 94 47 69 28 73 92 13 86 52 17 77 04 89 55 40", 
" 04 52 08 83 97 35 99 16 07 97 57 32 16 26 26 79 33 27 98 66", 
" 88 36 68 87 57 62 20 72 03 46 33 67 46 55 12 32 63 93 53 69", 
" 04 42 16 73 38 25 39 11 24 94 72 18 08 46 29 32 40 62 76 36", 
" 20 69 36 41 72 30 23 88 34 62 99 69 82 67 59 85 74 04 36 16", 
" 20 73 35 29 78 31 90 01 74 31 49 71 48 86 81 16 23 57 05 54", 
" 01 70 54 71 83 51 54 69 16 92 33 48 61 43 52 01 89 19 67 48"}; 

cf = If[ConsQ@#,Flatten[#,Reverse],{}]&;
filter = Function[{p, l}, cf@Fold[If[p@#2, Cons[#2,#1],#1]&, Null, l]];
qs = Function[{l}, If[Length@l<=1, l, Join[qs@filter[Order[#, First@l] >= 0&, Rest@l], {First@l}, qs@filter[Order[#, First@l] < 0&, Rest@l]]]];
eleQ = Function[{e,c}, If[ConsQ@c, If[e===Car@c,True,eleQ[e,Cdr@c]], e===c]];
eleQl= Function[{e,l}, Or@@(e===#&/@l)];
union = Function[{l}, Fold[If[eleQl[#2,#1],#1,Append[#1,#2]]&,{},l]];
cands = Function[{h}, {h[[1]]+#[[1]],h[[2]]+#[[2]]}&/@{{1,0},{1,1},{0,1},{-1,1},{-1,0},{-1,-1},{0,-1},{1,-1}}];
allcands=Function[{path}, filter[Not[eleQl[#, path]]&, union[Join@@(cands/@path)]]];
expand=Function[{path}, Append[path,#]&/@allcands[path]];
ll = union[qs /@ Nest[(Join@@(expand /@ #))&, {{{0,0}}}, 3]];
ll = {{{0,0}, {0,1}, {0,2}, {0,3}}, {{0,0}, {1,1}, {2,2}, {3,3}}, {{0,0}, {1,0}, {2,0}, {3,0}}, {{0,0}, {-1,1}, {-2,2}, {-3,3}}};
maxf = Function[{l, f}, First@Fold[If[f[#2] > #1[[2]], {#2,f[#2]}, #1]&,{First@l,f[First@l]},Rest@l]];
<<"data.txt";
ss = (ToExpression /@ #)& /@ StrWords /@ ss;
get = Function[{i,j}, If[1<=i<=20&&1<=j<=20, ss[[i,j]],0]];
pt = Function[{i,j},maxf[Function[{o},get[i+o[[1]],j+o[[2]]]]/@#&/@ll,Times@@#&]];
maxf[pt @@ # & /@ ({Quotient[#,20]+1, Mod[#,20]+1}& /@ Range[0, 399]), Times@@#&]

(** prob. 12 **)

(** prob. 13 **)
<<"data.txt";
Total @ ss

(** prob. 14 **)
iter = If[EvenQ @ #, Quotient[#, 2], 3 # + 1]&;
getlen0 = Function[{n}, getlen[iter @ n] + 1];
getlenc[1] = 1;
storelen=Function[{n, l}, getlenc[n]=l];
getlen = Function[{n}, If[Head@getlenc[n]==Integer,getlenc[n],storelen[n,getlen0[n]]]];
max=0;mn = 0; For[i=1,i<1000000,i++,t=getlen@i;If[max<t,max=t;mn=i]]//Timing//First

(** prob. 16 **)
    ToExpression /@ (Characters@ToString[2^1000]) // Total

(** prob. 20 **)
    ToExpression /@ (Characters@ToString[100!]) // Total

(** prob. 25 **)
    n=1; While[Length@Characters@ToString@fib5[n] < 1000, n++]; n

(** prob. 15 **)
genlst = Function[{n}, n ^ # & /@ Range[2, 100]];
merge0 = Function[{l, new, acc}, Which[ConsQ @ l === False && ConsQ @ new === False, acc, ConsQ @ new === False, merge0[Cdr@l,Null,Cons[Car@l,acc]], ConsQ @ l === False, merge0[Null, Cdr@new,Cons[Car@new,acc]], Car@l===Car@new, merge0[l, Cdr@new, acc], Car@l > Car@new, merge0[l, Cdr@new, Cons[Car@new,acc]], Car@l < Car@new, merge0[Cdr@l, new, Cons[Car@l,acc]]]];
merge = Function[{l, new}, Flatten[merge0[Cons[Sequence@@l, Null], Cons[Sequence@@new, Null], Null], Reverse]];
t = {0}; 
Function[{n}, t = merge[t, genlst@n]] /@ Range[2, 100] // Timing // First // Print;
Length@t - 1

(** prob. 48 **)
    StringJoin@@((Total[#^#&/@Range[1000]] // ToString // Characters // Reverse)[[Range[10]]] // Reverse)

(** prob. 340 **)
a = 21^7;
b = 7^21;
c = 12^7;
f = Function[{n}, If[Head@fc[n]===Integer,fc[n], fc[n]=If[n > b, n - c, f[a + f[a + f[a + f[a + n]]]]]]];

