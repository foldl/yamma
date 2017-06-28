(*****************

*******************)
fac1 = If[# < 2, 1, # #0[# - 1]]&;
fac3 = If[# < 2, 1, # fac3[# - 1]]&;
fac2 = If[#1 < 2, #2, #0[#1 - 1, #1 #2]]&;

fib1 = If[# < 2, 1, #0[# - 1] + #0[# - 2]]&;
fib20 = If[#3 > 0, Print[#2]; #0[#2, #1 + #2, #3 - 1], #2]&;
fib20 = If[#3 > 0, #0[#2, #1 + #2, #3 - 1], #2]&;
fib2 = fib20[1, 1, # - 2]&;

FoldLeft  = Fold;
FoldRight = Function[{f, acc, lst}, If[Length[lst] > 0, FoldRight[f, f[acc, Last @ lst], Most @ lst], acc]];
fMapThread0 = Function[{f, left, right, acc}, If[Length[left] > 0, 
                                                 fMapThread0[f, Rest @ left, Rest @ right, Append[acc, f[First @ left, First @ right]]], 
                                                 acc]];
fMapThread = Function[{f, lst}, fMapThread0[f, First @ lst, Last @ lst, {}]];
fNestList0 = Function[{f, x, n, Acc}, If[n > 0, 
                                            temp = f[x]; 
                                            fNestList0[f, temp, n - 1, Append[Acc, temp]]
                                            , 
                                            Acc]];
fMapIndexed = fMapThread[#1, {#2, Range[Length @ #2]}]&;                                            
fNestList = Function[{f, x, n}, fNestList0[f, x, n, {x}]];
fTotal = Plus @@ # &;
fSelect = Function[{lst, f}, Fold[If[f[#2] === True, Append[#1, #2], #1]&, {}, lst]];
fFixedPoint0 = Function[{f, x, nx}, If[nx == x, x, fFixedPoint0[f, nx, f[nx]]]];
fFixedPoint = Function[{f, x}, fFixedPoint0[f, x, f[x]]];
fFixedPointList0 = Function[{f, x, nx, acc}, If[nx == x, acc, fFixedPointList0[f, nx, f[nx], Append[acc, nx]]]];
fFixedPointList = Function[{f, x}, fFixedPointList0[f, x, f[x], {}]];
iPi = 3.141592653589793238462643383279502884197169399375105820974944592307816406286208998628034825342117067982148086;
fSign = If[EvenQ @ #, 1, -1]&;
fSin2 = Function[{x}, 1.0`80 fTotal[Map[fSign @ # x ^ (2 # + 1) / Factorial[2 # + 1] &, Range[0, 16]]]]; (* now, x is in [0, Pi/2) *)
fSin1 = Function[{x}, If[x > iPi / 2, fSin2[iPi - x], fSin2[x]]];
fSin0 = Function[{x}, If[x > iPi, - fSin1[x - iPi], fSin1[x]]];
fSin = Function[{x}, fSin0 @ Mod[x, 2 iPi]];

u16tos = If[# >= 32768, # - 65536, #]&;
digitToStr = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", 
              "A", "B", "C", "D", "E", "F","G","H","I","J","K",
              "L","M","N","O","P","Q","R","S","T","U","V","W",
              "X","Y","Z"}[[# + 1]]&;
baseForm0 = Function[{v, acc, base}, 
                     temp = Quotient[v, base]; 
                     If[v > 0, baseForm0[temp, StringJoin[digitToStr @ (v - temp base), acc], base], 
                               StringJoin[ToString @ base, StringJoin["^^", acc]]]]; 
baseForm = Function[{v, base}, If[v >= 0, baseForm0[v, "", base], StringJoin["-", baseForm0[- v, "", base]]]];
hexForm = baseForm[#, 16]&;
binForm = baseForm[#, 2]&;

f1 = Factorial[#1] / (Factorial[#1 - #2] Factorial[#2])&;
fMaken = Function[{n}, Map[{n, #}&, Range[1, n]]];
fMakeAll = Join @@ Map[fMaken, Range[10]];
Map[f1[First @ #, Last @ #]&, fMakeAll];

fSplit0 = Function[{f, run, Acc, l}, 
                    If[Length[l] > 0
                       ,
                       If[f[Last @ run, First @ l]
                            , 
                            fSplit0[f, Append[run, First @ l], Acc, Rest @ l]
                            ,
                            fSplit0[f, {First @ l}, Append[Acc, run], Rest @ l]]
                       ,
                       Append[Acc, run]]
                  ];
fSplit = Function[{l, f}, fSplit0[f, {First @ l}, {}, Rest @ l]];

fTakeWhile0 = Function[{H, T, f, Acc}, 
                        If[f[H], 
                            If[Length[T] > 0,
                                fTakeWhile0[First @ T, Rest @ T, f, Append[Acc, H]]
                                ,
                                Append[Acc, H]
                            ]
                            ,
                            Acc]];
fTakeWhile = Function[{l, f}, If[Length[l] > 0, fTakeWhile0[First @ l, Rest @ l, f, {}], {}]];

MyTest0 = Function[{n, x, v}, If[Mod[v, n] === 1, x, MyTest0[n, x + 1, 10 * v]]];
MyTest = Function[{n}, MyTest0[n, 1, 10]];

(* Mod[46, 2 iPi]*)

rev0 = Function[{rem, sofar}, 
                (* Print[rem, "    ", sofar]; *)
                If[ConsQ[rem]
                    , 
                    rev0[Cdr @ rem , ReplaceCdr[rem, sofar]]
                    ,
                    sofar]
                ];

rev1 = Function[{rem, sofar},
                (* Print[rem, "    ", sofar]; *)
                If[ConsQ[rem]
                    , 
                    rev1[Cdr @ rem, Cons[Car @ rem, sofar]]
                    ,
                    sofar]
                ];

(*
a = Cons[Sequence @@ (Range[5] ~ Join ~ {Nil})];
rev0[a, Nil]
a = Cons[Sequence @@ (Range[1000000] ~ Join ~ {Nil})];
Timing[rev0[a, Nil] // Car]
Print @ a;
Print @ b;

Timing[rev1[Cons[Sequence @@ (Range[100000] ~ Join ~ {Nil})], Nil] // Car]

*)

StrWords0 = Function[{acc},
                     chars = First @ acc;
                     If[Length @ chars > 0
                        ,
                        {{}, Append[Last @ acc, FromCharacterCode @ chars]}
                        ,
                        acc
                      ]
                      
];

StrWords = Function[{s},
    Last @ StrWords0 @ Fold[Function[{acc, cur},
                  If[cur == 9 || cur == 32 || cur == 10 || cur == 13
                     ,
                     StrWords0 @ acc
                     ,
                      {Append[First @ acc, cur], Last @ acc}
                   ]
                 ],
            {{}, {}},
            ToCharacterCode @ s]
];

cons2lst = If[ConsQ @ #, Flatten[#, Reverse], {}]&;

fPartition = Function[{lst, len},
    cons2lst @ Last @ Fold[Function[{acc, ele},
                            If[acc[[1, 2]] == len - 1
                                ,
                                {{Null, 0}, Cons[cons2lst @ Cons[ele, acc[[1, 1]]], Last @ acc]}
                                ,
                                {{Cons[ele, acc[[1, 1]]], acc[[1, 2]] + 1}, Last @ acc}
                            ]
                          ], {{Null, 0}, Null}, lst]
];

Unprotect[MapThread, Total, MapIndexed, Riffle, UpperCase, Partition, Max, Fibonacci];
MapThread = fMapThread;
MapIndexed = fMapIndexed;
Total = Plus @@ #&;
Max = Fold[If[(#1 < #2) === True, #2, #1]&, First @ #, Rest @ #]&;
Fibonacci = fib2;
ClearAttributes[Fibonacci, {ReadProtected}];
Protect[MapThread, Total, MapIndexed, Riffle, UpperCase, Partition, Max, Fibonacci];

mmM = {{#1[[1, 1]] * #2[[1, 1]] + #1[[1, 2]] * #2[[2, 1]], 
        #1[[1, 1]] * #2[[1, 2]] + #1[[1, 2]] * #2[[2, 2]]}, 
       {#1[[2, 1]] * #2[[1, 1]] + #1[[2, 2]] * #2[[2, 1]], 
        #1[[2, 1]] * #2[[1, 2]] + #1[[2, 2]] * #2[[2, 2]]}}&;

mpow = Function[{n, acc, odd}, Which[n == 1, mmM[acc, odd]
                                      , 
                                      EvenQ @ n, mpow[Quotient[n, 2], mmM[acc, acc], odd],
                                      mpow[n - 1, acc, mmM[acc, odd]]]];

fib5 = If[# <= 2, 1, Total @ First @ mpow[# - 2, {{1, 1}, {1, 0}}, {{1, 0}, {0, 1}}]]&;

(*
F[2k]   = F[k]*(F[k]+2F[k-1])
F[2k+1] = (2F[k]+F[k-1])*(2F[k]-F[k-1]) + 2*(-1)^k
fib6 = 

*)

<<"showpi.m";

(*
mp=: +/ . *

 fib0 =: $: (_1 + > {.; > 1 & {; mp / > }.) ` mp / > }. ` $: (2 %~ > {.; (mp ]) > 1 {; {:) @. ((#. @ ((0 = (2 & |)), (=&1))) @ > @ {.) > {.)
 fib  =: 1: ` fib0 ((-&2); (2 2 $ 1 1 0 1); (2 2 $ 1 0 0 1)) @. (>&2) 
 (#. @ ((0 = (2 & |)), (=&1))) "0 i. 9

((-&2); (2 2 $ 1 1 0 1); (2 2 $ 1 0 0 1)) 20

midamble = ToExpression /@ Flatten @ Cdr @ Cdr @ Cdr @ (Cons @@ (Characters @ binForm[16^^B2AC420F7C8DEBFA69505981BCD028C3]));
toKI = Which[Mod[#, 4] == 0, 1, Mod[#, 4] == 1, I, Mod[#, 4] == 2, -1, True, -I]&;
(midamble[[#]] toKI[#] ) & /@ Range[128]

*)

(*<< "morse.m";*)


