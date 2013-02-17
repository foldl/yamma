SCALE = 10000;
ARRINIT = 2000;

pidigits = Function[{digits}, 
    Function[{i}, arr[i] = ARRINIT] /@ Range[0, digits];
    R = 0;
    carry = 0;
    For[i = digits, i > 0, i -= 14,
        sum = Fold[Function[{acc, j},
            qr = QuotientRemainder[acc * j + SCALE * arr[j], j * 2 - 1];
            arr[j] = qr[[2]];
            qr[[1]]
            ], 0, Range[i, 1, -1]];
        qr = QuotientRemainder[sum, SCALE];
        R *= SCALE;
        R += carry + qr[[1]];
        carry = qr[[2]];
    ];
    R
];

pidigits0 = Function[{digits}, 
    Function[{i}, arr[i] = ARRINIT] /@ Range[0, digits];
    R = 0;
    carry = 0;
    For[i = digits, i > 0, i -= 14,
        sum = 0;
        For[j = i, j > 0, --j,
            qr = QuotientRemainder[sum * j + SCALE * arr[j], j * 2 - 1];
            arr[j] = qr[[2]];
            sum = qr[[1]];
        ];
        qr = QuotientRemainder[sum, SCALE];
        R *= SCALE;
        R += carry + qr[[1]];
        carry = qr[[2]];
    ];
    R
];

ShowDig = Function[{V}, First @ Nest[{(ToString @ Mod[#[[2]], 10]) <> #[[1]], Quotient[#[[2]], 10]}&, {"", V}, 4]];

test = Function[{},
  For[i = 0, i < 5, i++,
  Print[DownValues[arr]];
        arr[i] = i;
        Print[DownValues[arr]];
        ]
];
