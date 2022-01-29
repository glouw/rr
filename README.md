# The ROMAN II Programming Language

Roman II is a dynamic programming language inspired by C and Python
written in less than 5000 lines of GNU-C99.


```
Fib(n)
{
    if(n == 0) {
        ret 0;
    }
    elif((n == 1) || (n == 2)) {
        ret 1;
    }
    else {
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
-d: Output byte code and .data segment then terminate
```

## Types

Roman II supports the following types:

* number (double precision)
* string
* queue (arrays with O1 prepending and appending)
* map (O1 insertion and deletion)
* booleans
* function pointers
* files

## Built in functions

Roman II comes packed with the following functions:

```
    Sort(queue, comparator)
    Good(file)
    Assert(boolean)
    Keys(map)
    Copy(value)
    Open(file, mode)
    Read(file, mode)
    Refs(value)
    Len(value)
    Del(queue, index)
    Floor(number)
    Search(queue<sorted>, key, comparator)
    File()
    Line()
    Exit(number)
    Type(value)
    Print(string)
    String(value)
    Time()
    Srand(number)
    Rand()
    Write(file, string)
```

## A Quick Intro to Roman II

`Main()` is the entry point and must return a number value between 0 to 254.
255 is reserved for internal interpretor errors.

```
Main()
{
    ret 0
}
```

Variables are declared pascal style:

```
Main()
{
    number := 0;
    ret number;
}
```

Variables form expressions:

```
Main()
{
    value := (1 + 3) / 2.5;
    ret value;
}
```

Expressions can be reused as functions:

```
Math(a, b)
{
    ret (a + b) / 2.5;
}

Main()
{
    ret Math(1, 3);
}
```

Strings, numbers, booleans, maps, queues, and even `null` itself are refered to
as values. Values are interchangeable; they coexist within data structures:

```
Main()
{
    values := [ 0, 1, "string", {"key" : true }, null ];
    ret 0;
}
```

Iterating a queue is done with a for loop:

```
Main()
{
    queue := [ 99, 88, 77 ];
    for(i := 0; i < Len(queue); i += 1)
    {
        Print(queue[i]);
    }
    ret 0;
}
```

Likewise iterating a map is done similarly with the Keys function:

```
Main()
{
    map := {
        "A" : 1,
        "B" : 2,
        "C" : 3,
    };
    keys := Key(map);
    for(i := 0; i < Len(keys); i += 1)
    {
        key := keys[i];
        Print("{} : {}" % [key, map[key]]);
    }
    ret 0;
}
```

String formatting can be expanded to print number width and decimal places:

```
Main()
{
    variable := 4.33;
    formatted := {5.10}" % variable;
    Print(formatted);
}
```

Sorting queues requires use of a comparator function.

```
Comparator(a, b)
{
    ret a < b;
}

Main()
{
    a := [ 0, 9, 3, 1 ];
    Sort(a, Compartor);
    Print(a);
    ret 0;
}
```
