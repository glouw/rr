# The ROMAN II Programming Language

Roman II is a dynamic programming language inspired by C and Python.


```
fib(n)
{
    if(n == 0) {
        ret 0;
    }
    elif((n == 1) || (n == 2)) {
        ret 1;
    }
    else {
        ret fib(n - 1) + fib(n - 2);
    }
}

main()
{
    print(fib(25));
    ret 0;
}
```

The Roman II runtime strives to be implemented in less than 5000 lines
of the GNU99 dialect of C.
