
(*a small J interpreter in Yamma*)
(*
    a[t, r, d, p]
*)

L=Length;F=Function;G=Range;H=Head;A=Hold;J=Join;
U={};o=ToCharacterCode[#][[1]]&;P=Print; S=Characters;Q=If;W=Which;
at=#[[1]]&;ar=#[[2]]&;ad=#[[3]]&;ap=#[[4]]&;ad0=#[[3,1]]&;ap0=#[[4,1]]&;
qp=97<=o@#<=122&;qv=H@#==Integer&;id=#&;
size=a[0,0,{0},{Q[ar@#>0,#[[3,1]],1]}]&;
iota=a[0,1,{ap0@#},G[0,ap0@#-1]]&;
box=a[1,0,{0},{#}]&;
sha=a[0,1,{ar@#},ad@#]&;
plus=a[0,ar@#,ad@#,MapThread[Plus,{ap@#,ap@#2}]]&;
take=F[{p,n,l},{{l},p[[G[n*l+1,n*l+n+1]]]}];
from=a@@{at@#2,ar@#2-1}~J~take[ap@#2,ap0@#,Tr@Rest@ad@#2]&;
find=U;
f0=F[{l,n},l[[Mod[#,L@l]+1]]&/@G[0,n-1]];
fill=F[{l,nx},{nx,f0[l,Tr@nx]}];
rsh=a@@{at@#2,Q[ar@#2>0,ad0@#,1]}~J~fill[ap@#2,ap@#]&;
cat=a[at@#2,1,{L@ap@#+L@ap@#2},ap@#~J~ap@#2]&;

MapIndexed[F[{n,i},vt[n]=i],S["+{~<#,"]];
vm=A[id,size,iota,box,sha,U];
vd=A[plus,from,find,U,rsh,cat];
noun=Q[48<=o@#<=57,a[0,0,{1},{o@#-48}],U]&;
verb=Q[H@vt@#==vt,U,vt@#]&;

ex=F[{a,d},W[qp@a,W[ConsQ@d&&Car@d=="=",st[a]=ex@Cdr@d,ex@Cons[st[a],d]],d==U,a,qv@a,vm[[a]]@ex@d,vd[[Car@d]][a,ex@Cdr@d]]][Car@#,Cdr@#]&;
PS=F[{l},P@(StringJoin@@Riffle[ToString/@l," "])];
pr=F[{b},PS[f0[ad@b,ar@b]];Q[at@b>0,P["<"];pr/@ap@b,PS@ap@b]];
wd=F[{s},Fold[F[{so,e},Cons[W[t=noun@e;H@t==a,t,t=verb@e;qv@t,t,e],so]],U,Reverse@S@s]];

P["j is ready..."];m=Q[#=!=U,pr@ex@#;m[#2]]&[wd@InputString@"    ",#]&;m[0];

(*
e2=F[{a,p,q,k},W[p=="=",Q[qp@#,st[#]=#2]&[q,a];e0[a,k],e0[vd[[p]][q,a],k]]];
e1=F[{a,p,q},W[q==U||vd[[p]]==U||qv@Car@q,e0[vm[[p]][a],q],e2[a,p,Car@q,Cdr@q]]];
e0=F[{a,p},W[qp@a,e0[st[a],p],p==U,a,e1[a,Car@p,Cdr@p]]];ex=e0[Car@#,Cdr@#]&;
wd=F[{s},Fold[F[{so,e},Cons[W[t=noun@e;H@t==a,t,t=verb@e;qv@t,t,e],so]],U,S@s]];
*)
