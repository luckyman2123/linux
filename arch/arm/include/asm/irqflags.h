#ifndef __ASM_ARM_IRQFLAGS_H
#define __ASM_ARM_IRQFLAGS_H

#ifdef __KERNEL__

#include <asm/ptrace.h>

/*
 * CPU interrupt mask handling.
 */
#ifdef CONFIG_CPU_V7M
#define IRQMASK_REG_NAME_R "primask"
#define IRQMASK_REG_NAME_W "primask"
#define IRQMASK_I_BIT	1
#else
#define IRQMASK_REG_NAME_R "cpsr"
#define IRQMASK_REG_NAME_W "cpsr_c"
#define IRQMASK_I_BIT	PSR_I_BIT
#endif

#if __LINUX_ARM_ARCH__ >= 6

// comment by Clark:: __asm__　__volatile__("Instruction List" : Output : Input : Clobber/Modify);  ::2021-3-31
/* 
	http://www.blogjava.net/bacoo/archive/2012/05/01/377107.html
	
	http://emb.hqyj.com/Column/Column28.htm: 

	comment by Clark:: Instruction List 是汇编指令序列。它可以是空的，比如：__asm__ __volatile__(""); 或 __asm__ ("");都是完全合法的内联汇编表达式，
只不过这两条语句没有什么意义。但并非所有Instruction List 为空的内联汇编表达式都是没有意义的，比如：__asm__ ("":::"memory");
就非常有意义，它向GCC 声明：“内存作了改动”，GCC 在编译的时候，会将此因素考虑进去。 当在"Instruction List"中有多条指令的时候，
可以在一对引号中列出全部指令，也可以将一条 或几条指令放在一对引号中，所有指令放在多对引号中。如果是前者，可以将每一条指令放在一行，
如果要将多条指令放在一行，则必须用分号（；）或换行符（\n）将它们分开. 综上述：（1）每条指令都必须被双引号括起来 (2)两条指令必须用换行或分号分开。  

Output 用来指定当前内联汇编语句的输出

如果一个内联汇编语句的Clobber/Modify域存在"memory"，那么GCC会保证在此内联汇编之前，如果某个内存的内容被装入了寄存器，
那么在这个内联汇编之后，如果需要使用这个内存处的内容，就会直接到这个内存处重新读取，而不是使用被存放在寄存器中的拷贝。
因为这个 时候寄存器中的拷贝已经很可能和内存处的内容不一致了。
这只是使用"memory"时，GCC会保证做到的一点，但这并不是全部。因为使用"memory"是向GCC声明内存发生了变化，而内存发生变化带来的影响并不止这一点。

@ 应该是注释

__volatile__是GCC关键字volatile的宏定义：

#define __volatile__ volatile
__volatile__ 或volatile是可选的, 你可以用它也可以不用它. 如果你用了它, 则是向GCC声明“不要动我所写的Instruction List, 
我需要原封不动的保留每一条指令”, 否则当你使用了优化选项 (-O) 进行编译时, GCC将会根据自己的判断决定是否将这个内联汇编表达式中的指令优化掉.

%0 表示第一个参数，%1 表示第二个参数，依此类推，可以得到其它的参数，
这是老版本的gcc的方法了，好像只可以支持到10个参数。新版本的是可以直接在汇编中使用参数名来访问的，不过大部分源码还是使用%0的方法



I:IRQ中断;    F:FIQ中断

即关/开中断只是操作了CPSR中的中断标志位而已, 并没有去对GIC做操作，只是简单的不让CPU响应中断

CPSID	I	;PRIMASK=1，	;关中断

CPSIE	I	;PRIMASK=0，;开中断

CPSID 	F	;FAULTMASK=1,	;关异常
CPSIE   F	;FAULTMASK=0	;开异常



MRS{条件}   通用寄存器，程序状态寄存器（CPSR或SPSR）
MRS指令用于将程序状态寄存器的内容传送到通用寄存器中。该指令一般用在以下两种情冴： 

Ⅰ.当需要改变程序状态寄存器的内容时，可用MRS将程序状态寄存器的内容读入通用寄存器，修改后再写回程序状态寄存器。
Ⅱ.当在异常处理或进程切换时，需要保存程序状态寄存器的值，可先用该指令读出程序状态寄存器的值，然后保存。
指令示例：
MRS R0，CPSR  @传送CPSR的内容到R0
MRS R0，SPSR  @传送SPSR的内容到R0


MSR{条件}   程序状态寄存器（CPSR或SPSR）_<域>，操作数
MSR指令用亍将操作数的内容传送到程序状态寄存器的特定域中。其中，操作数可以为通用寄存器或立即数。<域>用于设置程序状态寄存器中需要操作的位，32位的程序状态寄存器可分为4个域：
位[31：24]为条件标志位域，用f表示；
位[23：16]为状态位域，用s表示；
位[15：8]为扩展位域，用x表示；
位[7：0]为控制位域，用c表示；

该指令通常用于恢复或改变程序状态寄存器的内容，在使用时，一般要在MSR指令中指明将要操作的域。
指令示例：
MSR CPSR，R0    @传送R0的内容到CPSR
MSR SPSR，R0    @传送R0的内容到SPSR
MSR CPSR_c，R0   @传送R0的内容到SPSR，但仅仅修改CPSR中的控制位域


orr
ORR 指令的格式为： 
ORR{条件}{S} 目的寄存器，操作数 1，操作数 2
ORR 指令用于在两个操作数上进行逻辑或运算，并把结果放置到目的寄存器中。操作数 1
应是一个寄存器，操作数 2 可以是一个寄存器，被移位的寄存器，或一个立即数。该指令常用于设置操作数 1 的某些位。 
指令示例： 
ORR   R0，R0，＃3 


BIC
BIC 位清除指令
指令格式：
BIC{cond}{S} Rd,Rn,operand2 
BIC指令将 Rn 的值与操作数 operand2 的反码按位逻辑"与", 结果存放到目的寄存器Rd 中.
指令示例: BIC R0,R0,#0x0F: 将R0最低4位清零, 其余位不变.




::2021-3-31
*/


#define arch_local_irq_save arch_local_irq_save
static inline unsigned long arch_local_irq_save(void)
{
	unsigned long flags;
	
	asm volatile(
		"	mrs	%0, " IRQMASK_REG_NAME_R "	@ arch_local_irq_save\n"
		"	cpsid	i"
		: "=r" (flags) : : "memory", "cc");
	return flags;
}

#define arch_local_irq_enable arch_local_irq_enable
static inline void arch_local_irq_enable(void)
{
	asm volatile(
		"	cpsie i			@ arch_local_irq_enable"
		:
		:
		: "memory", "cc");
}

#define arch_local_irq_disable arch_local_irq_disable
static inline void arch_local_irq_disable(void)
{
	asm volatile(
		"	cpsid i			@ arch_local_irq_disable"
		:
		:
		: "memory", "cc");
}

#define local_fiq_enable()  __asm__("cpsie f	@ __stf" : : : "memory", "cc")
#define local_fiq_disable() __asm__("cpsid f	@ __clf" : : : "memory", "cc")

#ifndef CONFIG_CPU_V7M
#define local_abt_enable()  __asm__("cpsie a	@ __sta" : : : "memory", "cc")
#define local_abt_disable() __asm__("cpsid a	@ __cla" : : : "memory", "cc")
#else
#define local_abt_enable()	do { } while (0)
#define local_abt_disable()	do { } while (0)
#endif
#else

/*
 * Save the current interrupt enable state & disable IRQs
 */
#define arch_local_irq_save arch_local_irq_save
static inline unsigned long arch_local_irq_save(void)
{
	unsigned long flags, temp;

	asm volatile(
		"	mrs	%0, cpsr	@ arch_local_irq_save\n"
		"	orr	%1, %0, #128\n"
		"	msr	cpsr_c, %1"
		: "=r" (flags), "=r" (temp)
		:
		: "memory", "cc");
	return flags;
}

/*
 * Enable IRQs
 */
#define arch_local_irq_enable arch_local_irq_enable
static inline void arch_local_irq_enable(void)
{
	unsigned long temp;
	asm volatile(
		"	mrs	%0, cpsr	@ arch_local_irq_enable\n"
		"	bic	%0, %0, #128\n"
		"	msr	cpsr_c, %0"
		: "=r" (temp)
		:
		: "memory", "cc");
}

/*
 * Disable IRQs
 */
#define arch_local_irq_disable arch_local_irq_disable
static inline void arch_local_irq_disable(void)
{
	unsigned long temp;
	asm volatile(
		"	mrs	%0, cpsr	@ arch_local_irq_disable\n"
		"	orr	%0, %0, #128\n"
		"	msr	cpsr_c, %0"
		: "=r" (temp)
		:
		: "memory", "cc");
}

/*
 * Enable FIQs
 */
#define local_fiq_enable()					\
	({							\
		unsigned long temp;				\
	__asm__ __volatile__(					\
	"mrs	%0, cpsr		@ stf\n"		\
"	bic	%0, %0, #64\n"					\
"	msr	cpsr_c, %0"					\
	: "=r" (temp)						\
	:							\
	: "memory", "cc");					\
	})

/*
 * Disable FIQs
 */
#define local_fiq_disable()					\
	({							\
		unsigned long temp;				\
	__asm__ __volatile__(					\
	"mrs	%0, cpsr		@ clf\n"		\
"	orr	%0, %0, #64\n"					\
"	msr	cpsr_c, %0"					\
	: "=r" (temp)						\
	:							\
	: "memory", "cc");					\
	})

#define local_abt_enable()	do { } while (0)
#define local_abt_disable()	do { } while (0)
#endif

/*
 * Save the current interrupt enable state.
 */
#define arch_local_save_flags arch_local_save_flags
static inline unsigned long arch_local_save_flags(void)
{
	unsigned long flags;
	asm volatile(
		"	mrs	%0, " IRQMASK_REG_NAME_R "	@ local_save_flags"
		: "=r" (flags) : : "memory", "cc");
	return flags;
}

/*
 * restore saved IRQ & FIQ state
 */
#define arch_local_irq_restore arch_local_irq_restore
static inline void arch_local_irq_restore(unsigned long flags)
{
	asm volatile(
		"	msr	" IRQMASK_REG_NAME_W ", %0	@ local_irq_restore"
		:
		: "r" (flags)
		: "memory", "cc");
}

#define arch_irqs_disabled_flags arch_irqs_disabled_flags
static inline int arch_irqs_disabled_flags(unsigned long flags)
{
	return flags & IRQMASK_I_BIT;
}

#include <asm-generic/irqflags.h>

#endif /* ifdef __KERNEL__ */
#endif /* ifndef __ASM_ARM_IRQFLAGS_H */
