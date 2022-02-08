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
-d: Output bytecode and .data segment then terminate. Do not run the
program.
```

Example bytecode output of the above Fib example follows:

```
!start:
	cal Main
	end
Fib:
	loc 0
	psh 0
	eql
	brf @l1
	psh 0
	sav
	fls
	jmp @l0
@l1:
	loc 0
	psh 1
	eql
	cpy
	loc 0
	psh 2
	eql
	lor
	brf @l2
	psh 1
	sav
	fls
	jmp @l0
@l2:
	loc 0
	cpy
	psh 1
	sub
	spd
	cal Fib
	lod
	cpy
	loc 0
	cpy
	psh 2
	sub
	spd
	cal Fib
	lod
	add
	sav
	fls
@l0:
	pop 1
	psh null
	sav
	ret
Main:
	psh 25
	spd
	cal Fib
	lod
	prt
	pop 1
	psh 0
	sav
	fls
	psh null
	sav
	ret

instructions 57 : labels 6
.data:
 0 :  0 : 0.000000
 1 :  0 : 1.000000
 2 :  0 : 2.000000
 3 :  0 : null
 4 :  0 : 25.000000
```
