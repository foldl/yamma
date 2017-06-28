(*
    VIM: this is yamma.    
*)

morseMap = 
"A   ，！      \
 B   ！，，，    \
 C   ！，！，    \
 D   ！，，     \
 E   ，       \
 F   ，，！，    \
 G   ！！，     \
 H   ，，，，    \
 I   ，，      \
 J   ，！！！    \
 K   ！，！     \
 L   ，！，，    \
 M   ！！      \
 N   ！，      \
 O   ！！！     \
 P   ，！！，    \
 Q   ！！，！    \
 R   ，！，     \
 S   ，，，     \
 T   ！       \
 U   ，，！     \
 V   ，，，！    \
 W   ，！！     \
 X   ！，，！    \
 Y   ！，！！    \
 Z   ！！，，    \
 1   ，！！！！   \
 2   ，，！！！   \
 3   ，，，！！   \
 4   ，，，，！   \
 5   ，，，，，   \
 6   ！，，，，   \
 7   ！！，，，   \
 8   ！！！，，   \
 9   ！！！！，   \
 0   ！！！！！   \
 ?   ，，！！，，  \
 .   ，！，！，！  \
 ,   ！！，，！！  \
 (   ！，！！，   \
 )   ！，！！，！  \
 ！   ！，，，，！  \
 +   ，！，！，   \
 *   ！，，！    \
 /   ！，，！，   \
 \"  ，！，，！，";

morseEncode = If[Head @ morseEncode0[#] == String, morseEncode0[#], "????"] &;
morseDecode = If[Head @ morseDecode0[#] == String, morseDecode0[#], "`" <> # <> "`"] &;

setup = Function[{words},
    Map[Function[{item},
        (morseEncode0[#1] = #2; morseDecode0[#2] = #1) & @@ item
    ], Partition[words, 2]];
    Null
];

morseMap = setup @ StrWords @ morseMap;

EncodeStr = StringJoin @@ Riffle[(morseEncode[#]& /@ Characters[ToUpperCase @ #]), "  "] &;
DecodeStr = StringJoin @@ (morseDecode[#]& /@ StrWords[#]) &;

Print["SOS: ", EncodeStr @ "SOS"];
