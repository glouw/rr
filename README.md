# The ROMAN II Programming Language

Roman II is a dynamically typed programming language inspired by C, Python, and Pascal.

Roman II is comprised of a recursive descent compiler, a virtual machine, and a garbage collector.

Roman II strives to be dependency free.

Roman II strives to be written in 5000 lines or less in one file titled `roman2.c`
with the GNU11 dialect of C.

Roman II strives to match the runtime performance and type system of idiomatic Python.

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

### Value Types

Aside from numbers which are of double precision, Roman II supports maps,
queues, files, strings, booleans, and pointer types which may point to
functions or variables.

#### Numbers

Variable assignment with Roman II is done with the `:=` operator.
Variables are mutable and can be reassigned:

```
Main()
{
    a := 1;
    a = 2;
    Print(a);
    ret 0;
}
```

Operators compatible with numbers include:

```
+  : Addition
-  : Subtraction
/  : Division
*  : Multiplication
%  : Floating Point Modulus
** : Power Of
%% : Integer Modulus
// : Integer Division
== : Equal To
!= : Not Equal To
<= : Less Than or Equal To
>= : Greater Than or Equal To
>  : Greater Than
<  : Less Than
```

Each operator supports its relational variant: `+=`, `-=`, `/=`, `*=`, `%=`, `**=`, `%%=`, `//=`.

#### Queues

Queues, also known as lists (with O(1) front and back operations!), can store value types.

```
Main()
{
    queue := [0, 1, 2, 3, 4];
    Print(queue);
    ret 0;
}
```

Queues can be appended to, and prepended to, with the `+=` and `-=` operators:


```
Main()
{
    queue := [0, 1, 2];
    queue += 3;
    queue += 4;
    queue -= -2;
    queue -= -1;
    Assert(queue == [-2, -1, 0, 1, 2, 3, 4]);
    ret 0;
}
```

Queues can access their elements with the `[]` operator:

```
Main()
{
    queue := [-1, 2, 3];
    Print(queue[1]);
    ret 0;
}
```

Queues can be sliced:

```
Main()
{
    queue := [0, 1, 2, 3]
    slice := Print(queue[1:3]);
    ret 0;
}
```

Queue slices are copies, so attempting to set a queue slice will have no effect:
```
Main()
{
    queue := [0, 1, 2, 3]
    queue[1:3] = [9, 9];
    Assert(queue == [0, 1, 2, 3]);
    ret 0;
}
```

The back of the queue can be accessed with the [-1] operator:
```
Main()
{
    queue := [0, 1, 2, 3]
    Assert(queue[-1] == 3);
    ret 0;
}
```

Elements can be removed from a queue with the `Del` keyword:

```
Main()
{
    queue := [0, 1, 2, 3];
    Del(queue,  0); # Pop front.
    Del(queue, -1); # Pop back.
    Assert(queue == [1, 2]);
    ret 0;
}
```

Finally, queues can be iterated over with the `foreach` loop.
Indexing via `foreach` is done by reference.

```
Main()
{
    queue := [0, 1, 2, 3];
    foreach(x : queue)
    {
        x += 1;
    }
    Assert(queue == [1, 2, 3, 4]);
    ret 0;
}
```

#### Strings

Strings contain an array of characters and can be indexed and modified like a queue:

```
Main()
{
    string := "the roman II Programming LanguagE";
    string[0] = "T";
    string[4] = "R";
    string[-1] = "e";
    ret 0;
}
```

To the programmer there is no notion of a `char` type in Roman II. Setting a
string element to a string longer than 1 will overwrite that character and the
characters following:

```
Main()
{
    ip := "192.168.10.0";
    ip[0] = "111";
    Assert(ip == "111.168.10.0");
    ret 0;
}
```

Strings can be appended to with the `+` operator.
Strings can be numerically compared with the `-` operator (eg. C's `strcmp`).
String elements can be deleted with the `Del` keyword. String element deletion
is O(1) from the back and O(N) from the middle and front.

```
Main()
{
    string := "Roman II";
    Del(string, -1);
    Del(string, -1);
    Del(string, -1);
    Assert(string == "Roman");
    ret 0;
}
```

Strings can be formatted with the `%` operator:

```
Main()
{
    formatted := "number : {5.2}, string : {}" % [1, "string"];
    Print(formatted);
    ret 0;
}
```

#### Maps

Maps strictly associate strings with a value type:

```
Main()
{
    map := {
        "string" : 1,
        "roman"  : 2
    };
    Assert(map["string"] == 1);
    ret 0;
}
```

Short hand, C struct style map initialization is supported: 

```
Main()
{
    map := {
        .string : 1,
        .roman : 2,
    };
    Assert(map.string == 1);
    ret 0;
}
```

Value types are interchangable. A map can associate strings to queues
just as a queue can hold maps and queues:

```
Main()
{
    map := {
        .roman : {
            .revision : 2,
            .queue : [0, 1, 2, 3, {}, []]
        },
    };
    Assert(map.roman.revision == 2);
    ret 0;
}
```
New keys can be inserted into a preexisting map with the `:=` operator.
Pre-existing keys can be modified with the `=` operator. Should a key
not exist, the `=` operator is a no-op.

```
Main()
{
    map := {}
    map["key"] := 1;
    map["key"] = 2;
    map.door := 3;
    map.door = 4;
    map.ceiling = 99;
    Assert(map.key == 2);
    Assert(map.door == 4); # No-op.
    Assert(map.ceiling == null);
    ret 0;
}
```

Like queues, any map element can be deleted with the `Del` keyword.

```
Main()
{
    map := { .key : 1 };
    Del(map, "key");
    Assert(map.key == null);
    ret 0;
}
```

Caution, values of type `null` can be inserted into maps.

Two maps can be merged with the `+` operator.

Maps can be sliced like queues. Like a queue slice, The last element of a
map slice is not included.

```
Main()
{
    map := {
        "a" : 1,
        "b" : 2,
        "c" : 3,
        "d" : 4,
        "e" : 5,
    };  
    Assert(map["b" : "d"] == {
        "b": 2,
        "c": 3
    });
    ret 0;
}
```

#### Boolean Expressions

Boolean expression are pascal-like requiring encolsed parenthesis
per boolean expression. Boolean expressions do not short circuit.

```
Main()
{
    boolean := (1 < 2) && (1 < 2 + 3) || (0 < 1); # Beware, no short circuiting occurs.
    ret 0;
}
```

DOCS TODO:

// ### Loops and Control Flow
// 
// Roman2 includes the standard `for` and `while` loop inspired by C,
// as well as a queue iterator dubbed `foreach`. Looping with `foreach`
// iterates by reference.
// 
// ```
// Main()
// {
//     for(i := 0; i < 10; i += 1)
//     {
//         if(i == 0)
//         {
//             continue;
//         }
//         elif(i == 1)
//         {
//             break;
//         }
//         else
//         {
//             while(true)
//             {
//                 break;
//             }
//         }
//     }
//     queue := [ 0, 1, 2, 3 ];
//     foreach(x : queue)
//     {
//         Print(x);
//         x += 1;
//     }
//     # queue is now [ 1, 2, 3, 4 ]
//     map := {
//         "a" : 1,
//         "b" : 2,
//         "c" : 3,
//     };
//     keys := Keys(map);
//     foreach(key : keys)
//     {
//         Print("{} : {}" % [key, map[key]]);
//     }
//     ret 0;
// }
// ```
// 
// ### Functions
// 
// Functions pass values by reference. Functions return `null` if
// no specific return valule is specified.
// 
// ```
// Increment(number)
// {
//     number := 1;
// }
// 
// Add(a, b)
// {
//     ret a + b;
// }
// 
// Main()
// {
//     a := Add(1, 3);
//     b := Add({ .x = 1 }, { .y = 2 })
//     c := Add([0, 1, 2, 3], [4, 5, 6, 7]);
//     d := 1;
//     Increment(d);
//     # d is now 2.
//     ret 0;
// }
// ```
// #### Pointers
// 
// Variables 
// 
// ```
// Main()
// {
//     f := 1;
//     g := &f;
//     Print(*g);
// 
//     h := { .key : 1 };
//     Print(h.key)
//     i := &h;
//     Print((*i).key);
//     Print(i@key); # Same as above but cleaner. See C's `->` arrow operator.
//     ret 0;
// }
// ```
// 
// ### Function Pointers
// 
// Functions can be pointed to with pointer syntaxing.
// 
// ```
// Fun(arg)
// {
//     Print(arg);
// }
// 
// Main()
// {
//     a := &Fun;
//     b := Fun;
//     a(42); # Automatically dereferenced.
//     b(42); # Same.
//     (*a)(42); # Manual dereference works as well.
//     ret 0;
// }
// ```
// 
// ### Sorting
// 
// Function pointers can be used as callbacks. For instance,
// a built in Qsort macro takes a function pointer comparator.
// 
// ```
// Compare(a, b)
// {
//     ret a < b;
// }
// 
// Test(Comparator, a, b)
// {
//     return Comparator(a, b);
// }
// 
// Main()
// {
//     Print(Test(Compare, 1, 2));
//     a := [ 0, 5, 3, 2 ];
//     b := [ "b", "c", "a", "f", "z"];
//     Qsort(a, Compare);
//     Qsort(b, Compare);
//     ret 0;
// }
// ```
// 
// ### Binary Searching
// 
// Likewise, a queue of values, be it numbers, strings, booleans, maps, or queues themselves,
// can be binary searched for a key if already sorted:
// 
// ```
// Compare(a, b)
// {
//     ret a < b;
// }
// 
// Diff(a, b)
// {
//     ret a - b;
// }
// 
// Main()
// {
//     a := [ "b", "c", "a", "f", "z"];
//     Qsort(a, Compare);
//     found := Bsearch(a, "b", Diff);
//     if(found != null)
//     {
//         Print(*found);
//     }
//     ret 0;
// }
// ```
// A queue of maps, should they be treated conventionally like C structs,
// can be sorted and searched:
// 
// ```
// Diff(a, b)
// {
//     ret a.key - b.key;
// }
// 
// Compare(a, b)
// {
//     ret a.key < b.key;
// }
// 
// Main()
// {
//     want := 99; 
//     a := [
//         { .key : "b", .value : 99 },
//         { .key : "a", .value :  2 },
//         { .key : "d", .value :  3 },
//         { .key : "c", .value :  4 },
//         { .key : "z", .value :  5 },
//         { .key : "f", .value :  6 },
//     ];  
//     Qsort(a, Compare);
//     b := Bsearch(a, { .key : "b" }, Diff);
//     Assert(b@value == 99);
//     ret 0;
// }
// ```
// ### Modules
// 
// Modules can be packaged and imported. Modules do not namespace and are recommended
// to include a suffix denoting the module name.
// 
// `Math.rr`
// ```
// Math_Add(a, b)
// {
//     ret a + b;
// }
// ```
// 
// `Main.rr`
// ```
// inc Math;
// 
// Main()
// {
//     ret Math_Add(-1, 1);
// }
// ```
// Module inclusions are akin to C's `#include` preprocessor directive, performing a source copy-paste,
// with the caveat that modules are processed once even with multiple inclusions of the same module.
// 
// ### Shared Object Libraries
// 
// Roman2 can call functions from native C shared objects libaries. Types sypported are `number`, `string`,
// and `bool`, mapping to types `double*`, `char*`, and `bool*`, respectively:
// 
// Math.c
// ```
// // gcc Math.c -o Math.so --shared -fpic
// 
// void Math_Add(double* self, double* other)
// {
//     *self += *other;
// }
// ```
// Main.rr
// 
// ```
// lib Math
// {
//     Math_Add(self, other);
// }
// 
// Main()
// {
//     a := 1;
//     Math_Add(a, 2);
//     Assert(a == 3); 
//     ret 0;
// }
// 
// ```
// 
// ### Built-In Macros
// 
// Built in macros are exposed by the compiler to present an include-free standard library.
// These macros cannot be pointed with pointer syntaxing.
// 
// A `value` can either be a `number`, `queue`, `bool`, `string`, `map`, `function`, or `file`.
// 
// ```
// Abs     (number)
// Acos    (number)
// All     (queue)
// Any     (queue)
// Asin    (number)
// Assert  (bool)
// Atan    (number)
// Bool    (string)
// Bsearch (queue, value, function)
// Ceil    (number)
// Copy    (value)
// Cos     (number)
// Del     (value, value);
// Exit    (number)
// Floor   (number)
// Keys    (map)
// Len     (value)
// Log     (number)
// Num     (string)
// Max     (value, value)
// Min     (value, value)
// Open    (file, string)
// Pow     (number)
// Print   (value)
// Qsort   (queue, function)
// Rand    ()
// Read    (file, number)
// Refs    (value)
// Sin     (number)
// Sqrt    (number)
// Srand   (number)
// Tan     (number)
// Time    () # Returns Microsecond uptime.
// Type    (value)
// Write   (file, string)
// ```
