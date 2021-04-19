#ifndef TYPECHECK_H_INCLUDED
#define TYPECHECK_H_INCLUDED

/*
 * Check at compile time that something is of a particular type.
 * Always evaluates to 1 so you may use it easily in comparisons.
 */

/* comment by Clark:: 
#define time_after(a,b)                /
        (typecheck(unsigned long, a) && /
         typecheck(unsigned long, b) && /
         ((long)(b) - (long)(a) < 0))

typecheck宏有两个参数，
第一个是一个类型，比如unsigned long，
第二个是一个变量，比如a。
它生成一个unsigned long类型的变量__dummy，
然后利用typeof生成和a同样类型的变量__dummy2，
比较__dummy和__dummy2的地址。
如果它们不是同样类型的指针比较，比如a不是unsigned long，
这时候编译器会有一个警告，让你注意到这个问题。

 //后面就是取两个变量的地址，然后比较，因为__dummy是 double类型 而__dummy2是 int 类型, 取地址后是两种不同类型的指针, 不同类型的指针不能比较, 会出现编译错误, 这种方式实现了类型的检查。
1;
//这个是表达式最后的值。如果整个表达式没有错误，那么最后的1就是整个表达式的值。
//总结：整个宏定义其实是一个表达式，不是函数，无所谓返回值，这个值是这个表达式的值，复合表达式, 取最后一个表达式的值, 作为复杂表达式的值。所以这里整个表达式的值就是最后一个表达式的值，也就是1

::2021-3-31
*/

#define typecheck(type,x) \
({	type __dummy; \
	typeof(x) __dummy2; \
	(void)(&__dummy == &__dummy2); \
	1; \
})

/*
 * Check at compile time that 'function' is a certain type, or is a pointer
 * to that type (needs to use typedef for the function type.)
 */
#define typecheck_fn(type,function) \
({	typeof(type) __tmp = function; \
	(void)__tmp; \
})

#endif		/* TYPECHECK_H_INCLUDED */
