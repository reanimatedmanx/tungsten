//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
//@ runNoFTL

function foo(a, b) {
    var [a, b] = [b, a];
    return a - b;
}

var result = 0;
for (var i = 0; i < 1e7; ++i)
    result += foo(42, i);

if (result != 49999575000000)
    throw "Bad result: " + result;
