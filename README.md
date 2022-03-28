# The ROMAN II Programming Language

Roman II is a dynamically typed programming language inspired by C, Python, and Pascal.

Roman II strives to be written in less than 5000 lines of the GNU11 dialect of C,
comprising of a recursive descent compiler, virtual machine, and a garbage collector.

Roman II strives to match the runtime performance and type system of idiomatic Python
while maintaining the expressivity of C pointers.

Roman II strives to be highly modifiable and portable and is intended to be used as
general purpose scripting language for desktop, server, and embedded systems.


```
Fib(n)
{
    if(n == 0)
    {
        ret 0;
    }
    elif((n == 1) || (n == 2))
    {
        ret 1;
    }
    else
    {
        ret Fib(n - 1) + Fib(n - 2);
    }
}

Main()
{
    Print(Fib(25));
    ret 0;
}
```

## Installation

```
make install
```

## Usage

```
roman2 Main.rr
```
Flags passed to `roman2`:

```
-d: Output bytecode and .data segment then terminate. Do not run the program.
```

### Program Entry

Roman2 enters and starts execution from function `Main`. The `Main` function
must return a number.

```
Main()
{
    Print("hello, world");
    ret 0;
}
```

### Built-in Types

Roman2 includes basic types such as numbers, queues (lists with O(1)
front and back push and pop), maps, strings, and booleans.

```
Main()
{
    number := 4.0; # Double is the default type.
    queue := [ 1, 2, 3, 4 ];
    map := {
        "a" : 1.0,
        "b" : queue,
        "c" : { "key" : [ 3, 4, 5 ] },
    };
    string := "roman2";
    boolean := true;
    ret 0;
}
```

### Basic Operators

Roman2 includes an array of operators and relational operators
compatible with built-in types. Assignment is done with the
`:=` operator, creating the python analogous of a deep copy.

```
Main()
{
# -- NUMBERS

    a := 1;
    a = 2;
    1 + 1;
    1 - 1;
    1 / 1;
    1 * 1;
    1 % 1;
    1 ** 1; # Power of.
    1 %% 1; # Integer modulus.
    1 // 1; # Integer division.

# -- MAPS

    b := {
        "a" : 1,
        .b  : 1, # Same as `"b" : 1`
    };

    # Create if key does not exist.
    b["c"] := 1;
    b["c"] = 1;
    b.d := 1; # Same as `b["d"] := 1`
    b.d = 1;

    # Deletion.
    Del(b, "c");
    Del(b, "d");

    # Merging two maps.
    { "a" : 1 } + { "b" : 1 };

# -- QUEUES

    c := [ 0, 1, 2, 3, 4 ];
    c = [];

    # Appending.
    [ 1, 2, 3, 4 ] + [ 5, 6, 7 ]

    # Prepending.
    [ 1, 2, 3, 4 ] - [ -3, -2, -1 ];

    # Front / back modification.
    c[0] = 1;
    c[-1] = 1;

    # O(1) deletion.
    Del(c, 0);
    Del(c, -1);

    # O(N) deletion.
    Del(c, Len(c) / 2);

# -- STRINGS

    d := "hello, world";
    d = "";
    "string" + "-" + "appending";

    # Taking the difference - eg. C's strcmp function 
    "string" - "string";

    # Formatting.
    "{} {}" % [ 1, { "key" : 1 } ];

    # O(N) deletion.
    Del(d, 0);
    Del(d, Len(d) / 2);

    # O(1) deletion.
    Del(d, -1);

# -- RELATIONAL OPERATORS
    
    e := 1
    e += 1;
    e -= 1;
    e %= 1;
    e /= 1;
    e *= 1;
    e **= 1;
    e %%= 1;
    e /%= 1;

# -- POINTERS

    f := 1;
    g := &f;
    Print(*g);

    h := { .key : 1 };
    Print(h.key)
    i := &h;
    Print((*i).key);
    Print(i@key); # Same as above but cleaner. See C's `->` arrow operator.
    ret 0;
}
```
### Boolean Expressions

Boolean expression are pascal-like requiring encolsed parenthesis
per boolean expression. Boolean expressions do not short circuit.

```
Main()
{
    a := (true) && (true);
    b := !a;
    c := (a) || (b) || (true); # Does not short circuit.
    d := (1 < 2) && (1 < 2 + 3) || (0 < 1);
    1 < 2;
    1 <= 2;
    1 > 2;
    1 >= 2;
    1 == 2;
    1 != 2;
    ret 0;
}
```

### Loops and Control Flow

Roman2 includes the standard `for` and `while` loop inspired by C,
as well as a queue iterator dubbed `foreach`. Looping with `foreach`
iterates by reference.

```
Main()
{
    for(i := 0; i < 10; i += 1)
    {
        if(i == 0)
        {
            continue;
        }
        elif(i == 1)
        {
            break;
        }
        else
        {
            while(true)
            {
                break;
            }
        }
    }
    queue := [ 0, 1, 2, 3 ];
    foreach(x : queue)
    {
        Print(x);
        x += 1;
    }
    # queue is now [ 1, 2, 3, 4 ]
    map := {
        "a" : 1,
        "b" : 2,
        "c" : 3,
    };
    keys := Keys(map);
    foreach(key : keys)
    {
        Print("{} : {}" % [key, map[key]]);
    }
    ret 0;
}
```

### Functions

Functions pass values by reference. Functions return `null` if
no specific return valule is specified.

```
Increment(number)
{
    number := 1;
}

Add(a, b)
{
    ret a + b;
}

Main()
{
    a := Add(1, 3);
    b := Add({ .x = 1 }, { .y = 2 })
    c := Add([0, 1, 2, 3], [4, 5, 6, 7]);
    d := 1;
    Increment(d);
    # d is now 2.
    ret 0;
}
```

### Function Pointers

Functions can be pointed to with pointer syntaxing.

```
Fun(arg)
{
    Print(arg);
}

Main()
{
    a := &Fun;
    b := Fun;
    a(42); # Automatically dereferenced.
    b(42); # Same.
    (*a)(42); # Manual dereference works as well.
    ret 0;
}
```

### Sorting

Function pointers can be used as callbacks. For instance,
a built in Qsort macro takes a function pointer comparator.

```
Compare(a, b)
{
    ret a < b;
}

Test(Comparator, a, b)
{
    return Comparator(a, b);
}

Main()
{
    Print(Test(Compare, 1, 2));
    a := [ 0, 5, 3, 2 ];
    b := [ "b", "c", "a", "f", "z"];
    Qsort(a, Compare);
    Qsort(b, Compare);
    ret 0;
}
```

### Binary Searching

Likewise, a queue of values, be it numbers, strings, booleans, maps, or queues themselves,
can be binary searched for a key if already sorted:

```
Compare(a, b)
{
    ret a < b;
}

Diff(a, b)
{
    ret a - b;
}

Main()
{
    a := [ "b", "c", "a", "f", "z"];
    Qsort(a, Compare);
    found := Bsearch(a, "b", Diff);
    if(found != null)
    {
        Print(*found);
    }
    ret 0;
}
```
A queue of maps, should they be treated conventionally like C structs,
can be sorted and searched:

```
Diff(a, b)
{
    ret a.key - b.key;
}

Compare(a, b)
{
    ret a.key < b.key;
}

Main()
{
    want := 99; 
    a := [
        { .key : "b", .value : 99 },
        { .key : "a", .value :  2 },
        { .key : "d", .value :  3 },
        { .key : "c", .value :  4 },
        { .key : "z", .value :  5 },
        { .key : "f", .value :  6 },
    ];  
    Qsort(a, Compare);
    b := Bsearch(a, { .key : "b" }, Diff);
    Assert(b@value == 99);
    ret 0;
}
```
### Modules

Modules can be packaged and imported. Modules do not namespace and are recommended
to include a suffix denoting the module name.

Math.rr
```
Math_Add(a, b)
{
    ret a + b;
}
```

Main.rr
```
inc Math;

Main()
{
    ret Math_Add(0, 1);
}
```
Module inclusions are akin to C's `#include` preprocessor directive, performing a source copy-paste,
with the caveat that modules are processed once even with multiple inclusions of the same module.

### Built-In Macros

Built in macros are exposed by the compiler. These macros cannot be pointed to by pointers.

A `value` can either be a `number`, `queue`, `bool`, `string`, `map`, `function`, or `file`.

```
Abs     (number)
Acos    (number)
All     (queue)
Any     (queue)
Asin    (number)
Assert  (bool)
Atan    (number)
Bool    (string)
Bsearch (queue, value, function)
Ceil    (number)
Copy    (value)
Cos     (number)
Del     (value, value);
Exit    (number)
Floor   (number)
Keys    (map)
Len     (value)
Log     (number)
Num     (string)
Max     (value, value)
Min     (value, value)
Open    (file, string)
Pow     (number)
Print   (value)
Qsort   (queue, function)
Rand    ()
Read    (file, number)
Refs    (value)
Sin     (number)
Sqrt    (number)
Srand   (number)
Tan     (number)
Time    () # Returns Microsecond uptime.
Type    (value)
Write   (file, string)
```
