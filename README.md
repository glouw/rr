# The ROMAN II Programming Language

Roman II is a dynamic programming language inspired by C and Python.


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
