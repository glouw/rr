# The ROMAN II Programming Language

Roman II is a dynamic programming language with a naive mark and sweep garbage
collector, all written from the ground up in about 5000 lines of the GNU11 dialect of C.


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

## Program Entry

Roman II enters and starts execution from function `Main`. The `Main` function
must return a number.

```
Main()
{
    Print("hello, world");
    ret 0;
}
```

## Value Types

Aside from numbers which are of double precision, Roman II supports maps,
queues, files, strings, booleans, nulls, and pointers, the latter which may
point to functions or variables.

### Numbers

Variable assignment with Roman II is done with the `:=` operator.
Variables are mutable and can be reassigned:

```
Main()
{
    a := 1;
    a = 2;
    Assert(a == 2);
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
```

Boolean operators include:

```
== : Equal To
!= : Not Equal To
<= : Less Than or Equal To
>= : Greater Than or Equal To
>  : Greater Than
<  : Less Than
```

Each operator supports its relational variant: `+=`, `-=`, `/=`, `*=`, `%=`, `**=`, `%%=`, `//=`.

Operators and relational variants have optional support for `queue`, `map`, and `string` types.

### Queues

Queues (also known as lists with O(1) front and back operations), can store value types:

```
Main()
{
    queue := [0, 1, 2, 3, 4];
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
    Assert(queue[1] == 2);
    ret 0;
}
```

Queues can be sliced:

```
Main()
{
    queue := [0, 1, 2, 3]
    slice := Assert(queue[1:3] == [1, 2]);
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

Queues can be iterated over with the `foreach` loop.
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

Any value type may be inserted into a queue.

### Strings

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
    formatted := "number : {5.2}, string : {}" % [1, "Roman II"];
    Assert(formatted == "number : 1.00, string : Roman II");
    ret 0;
}
```

### Maps

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

Short hand, C struct style map notation is supported: 

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

Value types are interchangeable. A map can associate strings to queues
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
New keys can be inserted into a map with the `:=` operator.
Pre-existing keys can be modified with the `=` operator. Should a key
not exist, the `=` operator is a no-op.

```
Main()
{
    map := {}
    map["key"] := 1;
    map["key"] = 2;
    map.door := 3;
    map.door = 4; # No-op.
    map.ceiling = 99;
    Assert(map.key == 2);
    Assert(map.door == 4);
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

Values of type `null` can be inserted into maps. The `Exists` keyword
can be used to check for such `nulls` by checking for the existence of a key:

```
Main()
{
    map := { .key : null };
    Assert(Exists(map, "key"));
    ret 0;
}
```

Two maps can be merged with the `+` operator.

Maps can be sliced like queues. Like a queue slice, the last element of a
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

Maps can be iterated with the `Keys` keyword: 


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
    keys := Keys(map);
    foreach(key : keys)
    {
        map[key] += 1;
    }
    Assert(map == {
        .a : 2,
        .b : 3,
        .c : 4,
        .d : 5,
        .e : 6,
    });
    ret 0;
}
```

## Boolean Expressions

Boolean expression are pascal-like requiring enclosed parenthesis
per boolean expression. Operators `&&` and `||` evaluate expressions to
then left and then the right. Boolean expressions do not short circuit.

```
Main()
{
    boolean := (1 < 2) && (1 < 2 + 3) || (0 < 1); # Beware, no short circuiting occurs.
    ret 0;
}
```

## Loops and Control Flow

Standard `for` and `while` loops can loop expression blocks.
Continuing within a `for` loop will run it's advancement expression.

The following two loops are equivalent:

```
Main()
{
    for(i := 0; i < 10; i += 1)
    {
        Print(i);
    }
    ret 0;
}
```

```
Main()
{
    i := 0;
    while(i < 10)
    {
        Print(i);
        i += 1;
    }
    ret 0;
}
```

## Functions

Functions pass values by reference. Functions return `null` if
no specific return value is specified.

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
    Assert(a == 4);
    Assert(b == { .x = 1, .y = 2 });
    Assert(c == [0, 1, 2, 3, 4, 5, 6, 7]);
    Assert(d == 2);
    ret 0;
}
```

References passed to functions can be checked for aliasing with
the `?` operator to avoid a superfluous copies:

```
Set(value, with)
{
    if(value ? with)
    {
        ret;
    }
    else
    {
        value = with;
    }
}

Main()
{
    a := 1;
    Set(a, a);
    Assert(a == 1);
    ret 0;
}
```

### Pointers

Variables and functions can be pointed to with pointer syntaxing.
A variable pointer requires use of the address-of `&` operator, followed by
a dereference `*` operator:

```
Main()
{
    f := 1;
    g := &f;
    Assert(*g == 1);
    ret 0;
}
```

All variable types can be pointed to, but to ease pointer syntaxing
with maps using C struct notation the `@` operator can be employed:

```
Main()
{
    map := { .key : 1 };
    pointer := &map;
    Assert(map.key == 1)
    Assert((*pointer).key == 1);
    Assert(pointer@key == 1); # Same as above but a little easier on the programmer.
    ret 0;
}
```

Functions can also be pointed to with pointer syntaxing.

```
Fun(arg)
{
    Print(arg);
}

Main()
{
    a := &Fun;
    b := Fun;
    a(42); # `a` is automatically dereferenced.
    b(42); # Same as above.
    (*a)(42); # Manual dereferencing for `a` works as well.
    ret 0;
}
```

Function pointers can be used as callbacks.

```
Printer(text)
{
    Print(text);
}

Message(printer, text)
{
    printer(text);
}

Main()
{
    Message(Printer, "Hello, world!");
    ret 0;
}
```

## Sorting

Function pointers (in this case, a comparison function pointer)
are often used while quick sorting:

```
Compare(a, b)
{
    ret a < b;
}

Main()
{
    a := [0, 5, 3, 2];
    b := ["b", "c", "a", "f", "z"];
    Qsort(a, Compare);
    Qsort(b, Compare);
    ret 0;
}
```

## File Input and Output

Files can be opened, read, and written to, with built in calls `Open`, `Read`, and `Write`.

```
Main()
{
    file := Open("file.txt", "w");
    in := "testing!"
    Write(file, in);
    out := Read(file, Len(in));
    Assert(out == in);
    Assert(Len(file) == in);
    ret 0;
}
```

A file is automatically closed once it's reference count reaches 0. The length of
the file returned by `Len` is the number of bytes in the file.

## Value Length

Strings, numbers, and booleans can be compared with all boolean operators.
Other value types, such as queues, maps, files, use their length (size) for comparison.
The `Len` keyword returns the length (or size) of a queue, map, string, or file:

```
Main()
{
    Assert(Len("abc") == 3);
    Assert(Len([1,2,3]) == 3);
    Assert(Len({.a : 1}) == 1);
    file := Open("file.txt", "r");
    Assert(Len(file) == 42); # Assuming `file.txt` consists of 42 characters.
    ret 0;
}
```
## Binary Searching

As with sorting, a queue of values can be binary searched
for a key if the queue is already sorted. Bsearch requires
a difference function that returns 0 with a match.

```
Comp(a, b) { ret a < b; }
Diff(a, b) { ret a - b; }

Main()
{
    a := ["b", "c", "a", "f", "z"];
    Qsort(a, Compare);
    found := Bsearch(a, "b", Diff);
    if(found != null)
    {
        Assert(*found == "b");
    }
    ret 0;
}
```

A queue of maps can be sorted and searched with a minor
adjustment to the Comparison and Difference functions:

```
Comp(a, b) { ret a.key < b.key; }
Diff(a, b) { ret a.key - b.key; }

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

In the above example, string subtraction within `Diff` is analogous to C's `strcmp`,
returning 0 with a string match.

## Modules

Modules can be packaged and imported. Modules do not namespace and are recommended
to include a suffix denoting the module name:

Math.rr:
```
Math_Add(a, b)
{
    ret a + b;
}
```

Main.rr:
```
inc Math;

Main()
{
    ret Math_Add(-1, 1);
}
```
Module inclusions are akin to C's `#include` preprocessor directive, which performs
a source copy and paste. Modules are processed only once, even with multiple
inclusions of the same module.

Modules within directories can be included with the dot `.` operator. Dot operators
prefixing a module reference modules one up from the current directory:

```
inc ..Lib.Basic.Math; # "../../Lib/Basic/Math.rr"
```

## Shared Object Libraries

Roman II can call functions from native C shared objects libraries. Value types
supported are `number`, `string`, and `bool`, mapping to types `double*`,
`char*`, and `bool*`, respectively:

Math.c:
```
// gcc Math.c -o Math.so --shared -fpic

void Math_Add(double* self, double* other)
{
    *self += *other;
}
```
Main.rr:

```
lib Math
{
    Math_Add(self, other);
}

Main()
{
    a := 1;
    Math_Add(a, 2);
    Assert(a == 3); 
    ret 0;
}
```

## Built-In Keywords

Built in keywords are exposed by the compiler to present an include-free standard library.
These keywords cannot be pointed to with pointer syntaxing:

```
KEYWORD  ARGUMENTS

Abs      (number)
Acos     (number)
All      (queue)
Any      (queue)
Asin     (number)
Assert   (bool)
Atan     (number)
Bsearch  (queue, value, function)
Ceil     (number)
Copy     (value)
Cos      (number)
Del      (value, value);
Exists   (map, string);
Exit     (number)
Floor    (number)
Keys     (map)
Len      (value)
Log      (number)
Max      (value, value)
Min      (value, value)
Open     (file, string)
Pow      (number)
Print    (value)
Qsort    (queue, function)
Rand     ()
Read     (file, number)
Refs     (value)
Sin      (number)
Sqrt     (number)
Srand    (number)
Tan      (number)
Time     () # Microseconds.
Type     (value)
Write    (file, string)
```

## Garbage Collection

While most values free when their internal reference counts reaches 0,
values with cyclical references require the intervention of the garbage collector.
An example of a circular reference is that of two maps pointing to each other:

```
Main()
{
    a := {};
    b := {};
    # Ref count of `a` is now 1.
    # Ref count of `b` is now 1.

    a.pointer := &b;
    b.pointer := &a;
    # Ref count of `a` is now 2.
    # Ref count of `b` is now 2.

    ret 0;

    # Return decrements `a` and `b` ref counts by 1.
    # Ref count of `a` is now 1 and needs to be garbage collected.
    # Ref count of `b` is now 1 and needs to be garbage collected.
}
```

Roman II's garbage collector is tracked by an internal tracking value named `alloc_cap`.
This value is set to an arbitrary buffer value (eg. 1024 values) at startup.
Should the number of values allocated exceed `alloc_cap` all values are marked
as either reachable or unreachable and the unreachable values are sweeped (freed).
The new `alloc_cap` size is set to the current number of reachable values plus
the arbitrary buffer value (eg. 1024).

The garbage collector checks `alloc_cap` with every new local variable assignation.

## Constant Values

Objects marked with the `const` keyword escape garbage collection (eg. cyclical reference checks).
`const` objects may not hold pointers.


```
Main()
{
    path := "path/to/a/very/large/file.json"; # This is a 300MB JSON.
    file := Read(path, "r");
    const json := Value(Read(file, Len(file)));
    ret 0;
}
```

An ideal program will have all values initialized as `const` to escape the garbage collector.
