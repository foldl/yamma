CFG=Debug

CPP=gcc
LINK32=gcc

ifeq ($(CFG), Debug)
OUTDIR=./GccDebug
INTDIR=./GccDebug
CPP_PROJ=-c -g -pg -Wall -x c++ -D_DEBUG -o
LINK32_FLAGS=-pg -Wl,-lsupc++ -o
else
OUTDIR=./GccRelease
INTDIR=./GccRelease
CPP_PROJ=-c -O3 -Wall -x c++ -o
LINK32_FLAGS=-Wl,-lsupc++ -O3 -o
endif

ALL : $(OUTDIR)/yamma.exe

CLEAN :
	-@erase "$(INTDIR)\bif.obj"
	-@erase "$(INTDIR)\bif_list.obj"
	-@erase "$(INTDIR)\bif_math.obj"
	-@erase "$(INTDIR)\bif_cc.obj"
	-@erase "$(INTDIR)\bif_helper.obj"
	-@erase "$(INTDIR)\bif_sys.obj"
	-@erase "$(INTDIR)\big.obj"
	-@erase "$(INTDIR)\encoding.obj"
	-@erase "$(INTDIR)\eval.obj"
	-@erase "$(INTDIR)\expr.obj"
	-@erase "$(INTDIR)\hash.obj"
	-@erase "$(INTDIR)\matcher.obj"
	-@erase "$(INTDIR)\moi.obj"
	-@erase "$(INTDIR)\mm.obj"
	-@erase "$(INTDIR)\msg.obj"
	-@erase "$(INTDIR)\mstr.obj"
	-@erase "$(INTDIR)\num.obj"
	-@erase "$(INTDIR)\parser.obj"
	-@erase "$(INTDIR)\sym.obj"
	-@erase "$(INTDIR)\token.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\yamma.obj"
	-@erase "$(OUTDIR)\yamma.exe"
	-@erase "$(OUTDIR)\yamma.ilk"

$(OUTDIR) :
	mkdir -p "$(OUTDIR)"

{./kernel}.c{$(INTDIR)}.obj:: 
	$(CPP) $(CPP_PROJ) $@ $<

LINK32_OBJS= \
	$(INTDIR)/bif.obj \
	$(INTDIR)/bif_list.obj \
	$(INTDIR)/bif_math.obj \
	$(INTDIR)/bif_cc.obj \
	$(INTDIR)/bif_helper.obj \
	$(INTDIR)/bif_sys.obj \
	$(INTDIR)/big.obj \
	$(INTDIR)/encoding.obj \
	$(INTDIR)/eval.obj \
	$(INTDIR)/expr.obj \
	$(INTDIR)/hash.obj \
	$(INTDIR)/matcher.obj \
	$(INTDIR)/moi.obj \
	$(INTDIR)/mm.obj \
	$(INTDIR)/msg.obj \
	$(INTDIR)/mstr.obj \
	$(INTDIR)/num.obj \
	$(INTDIR)/parser.obj \
	$(INTDIR)/sym.obj \
	$(INTDIR)/token.obj \
	$(INTDIR)/kernel.obj \
	$(INTDIR)/yamma.obj

$(OUTDIR)/yamma.exe : $(OUTDIR) $(LINK32_OBJS)
	$(LINK32) $(LINK32_FLAGS)  $@ $(LINK32_OBJS) 

$(INTDIR)/yamma.obj: yamma.c ./kernel/bif.h ./kernel/moi.h ./kernel/assert.h 	\
   	./kernel/eval.h ./kernel/mm.h ./kernel/encoding.h ./kernel/matcher.h 		\
	./kernel/token.h ./kernel/msg.h ./kernel/sys_sym.h ./kernel/expr.h 			\
	./kernel/sym.h ./kernel/num.h ./kernel/hash.h ./kernel/sys_msg.h 			\
	./kernel/parser.h ./kernel/mtype.h ./kernel/mstr.h
	$(CPP) -fno-threadsafe-statics $(CPP_PROJ) $@ $<

$(INTDIR)/bif.obj: ./kernel/bif.c ./kernel/bif.h ./kernel/moi.h 				\
	./kernel/assert.h ./kernel/eval.h ./kernel/mm.h ./kernel/encoding.h 		\
	./kernel/matcher.h ./kernel/token.h ./kernel/msg.h ./kernel/sys_sym.h 		\
	./kernel/expr.h ./kernel/sym.h ./kernel/num.h ./kernel/hash.h 				\
	./kernel/sys_msg.h ./kernel/parser.h ./kernel/mtype.h ./kernel/mstr.h
	$(CPP) -fno-threadsafe-statics $(CPP_PROJ) $@ $<

$(INTDIR)/bif_list.obj: ./kernel/bif_list.c ./kernel/bif.h ./kernel/hash.h 		\
	./kernel/eval.h ./kernel/mm.h ./kernel/msg.h ./kernel/sys_sym.h 			\
	./kernel/expr.h ./kernel/num.h ./kernel/assert.h ./kernel/sys_msg.h 		\
	./kernel/mtype.h ./kernel/mstr.h
	$(CPP) $(CPP_PROJ) $@ $<

$(INTDIR)/bif_math.obj: ./kernel/bif_math.c ./kernel/bif.h ./kernel/hash.h 		\
	./kernel/eval.h ./kernel/mm.h ./kernel/msg.h ./kernel/sys_sym.h 			\
	./kernel/expr.h ./kernel/num.h ./kernel/assert.h ./kernel/sys_msg.h 		\
	./kernel/mtype.h ./kernel/mstr.h
	$(CPP) $(CPP_PROJ) $@ $<

$(INTDIR)/bif_cc.obj: ./kernel/bif_cc.c ./kernel/bif.h ./kernel/hash.h 			\
	./kernel/eval.h ./kernel/mm.h ./kernel/msg.h ./kernel/sys_sym.h 			\
	./kernel/expr.h ./kernel/num.h ./kernel/assert.h ./kernel/sys_msg.h 		\
	./kernel/mtype.h ./kernel/mstr.h ./kernel/bif_internal.h
	$(CPP) $(CPP_PROJ) $@ $<

$(INTDIR)/bif_sys.obj: ./kernel/bif_sys.c ./kernel/bif.h ./kernel/hash.h 		\
	./kernel/eval.h ./kernel/mm.h ./kernel/msg.h ./kernel/sys_sym.h 			\
	./kernel/expr.h ./kernel/num.h ./kernel/assert.h ./kernel/sys_msg.h 		\
	./kernel/mtype.h ./kernel/mstr.h ./kernel/bif_internal.h
	$(CPP) $(CPP_PROJ) $@ $<

$(INTDIR)/bif_helper.obj: ./kernel/bif_helper.c ./kernel/bif_internal.h 		\
	./kernel/hash.h ./kernel/eval.h ./kernel/mm.h ./kernel/msg.h 				\
	./kernel/sys_sym.h ./kernel/expr.h ./kernel/num.h ./kernel/assert.h 		\
	./kernel/sys_msg.h ./kernel/mtype.h ./kernel/mstr.h
	$(CPP) $(CPP_PROJ) $@ $<

$(INTDIR)/big.obj: ./kernel/big.c ./kernel/assert.h ./kernel/mtype.h 			\
	./kernel/encoding.h ./kernel/msg.h ./kernel/sys_sym.h ./kernel/expr.h 		\
	./kernel/sym.h ./kernel/num.h ./kernel/hash.h ./kernel/mm.h ./kernel/big.h 	\
	./kernel/mstr.h
	$(CPP) -fno-threadsafe-statics $(CPP_PROJ) $@ $<

$(INTDIR)/encoding.obj: ./kernel/encoding.c ./kernel/sys_char_init.inc 			\
	./kernel/hash.h ./kernel/sys_char_dec.inc ./kernel/assert.h ./kernel/mtype.h \
	./kernel/encoding.h ./kernel/mm.h ./kernel/mstr.h
	$(CPP) $(CPP_PROJ) $@ $<

$(INTDIR)/eval.obj: ./kernel/eval.c ./kernel/bif.h ./kernel/assert.h 			\
	./kernel/eval.h ./kernel/mm.h ./kernel/encoding.h ./kernel/msg.h 			\
	./kernel/sys_sym.h ./kernel/expr.h ./kernel/sym.h ./kernel/num.h 			\
	./kernel/hash.h ./kernel/mtype.h ./kernel/mstr.h
	$(CPP) $(CPP_PROJ)  $@ $<

$(INTDIR)/expr.obj: ./kernel/expr.c ./kernel/assert.h ./kernel/mm.h 			\
	./kernel/encoding.h ./kernel/msg.h ./kernel/sys_sym.h ./kernel/expr.h 		\
	./kernel/sym.h ./kernel/num.h ./kernel/hash.h ./kernel/mtype.h ./kernel/mstr.h
	$(CPP) -fno-threadsafe-statics $(CPP_PROJ) $@ -Wa,-adhln=$@.asm  $<

$(INTDIR)/hash.obj: ./kernel/hash.c ./kernel/hash.h ./kernel/assert.h 			\
	./kernel/mm.h ./kernel/mtype.h ./kernel/mstr.h
	$(CPP) $(CPP_PROJ) $@ $<

$(INTDIR)/matcher.obj: ./kernel/matcher.c ./kernel/bif.h ./kernel/hash.h 		\
	./kernel/eval.h ./kernel/mm.h ./kernel/matcher.h ./kernel/sys_sym.h 		\
	./kernel/expr.h ./kernel/sym.h ./kernel/num.h ./kernel/assert.h ./kernel/mtype.h ./kernel/mstr.h
	$(CPP) $(CPP_PROJ) $@ $<

$(INTDIR)/moi.obj: ./kernel/moi.c ./kernel/assert.h ./kernel/moi.h 				\
	./kernel/mtype.h ./kernel/encoding.h ./kernel/mm.h ./kernel/mstr.h
	$(CPP) $(CPP_PROJ) $@ $<

$(INTDIR)/mm.obj: ./kernel/mm.c ./kernel/assert.h ./kernel/mm.h 				\
	./kernel/encoding.h ./kernel/expr.h ./kernel/num.h ./kernel/hash.h 			\
	./kernel/mtype.h ./kernel/mstr.h
	$(CPP) $(CPP_PROJ) $@ $<

$(INTDIR)/msg.obj: ./kernel/msg.c ./kernel/assert.h ./kernel/mm.h ./kernel/msg.h \
	./kernel/expr.h ./kernel/num.h ./kernel/hash.h ./kernel/mtype.h ./kernel/mstr.h
	$(CPP) $(CPP_PROJ) $@ $<

$(INTDIR)/mstr.obj: ./kernel/mstr.c ./kernel/assert.h ./kernel/mm.h 			\
	./kernel/encoding.h ./kernel/mtype.h ./kernel/mstr.h
	$(CPP) $(CPP_PROJ) $@ $<

$(INTDIR)/num.obj: ./kernel/num.c ./kernel/assert.h ./kernel/mm.h 				\
	./kernel/encoding.h ./kernel/sys_sym.h ./kernel/expr.h ./kernel/num.h 		\
	./kernel/hash.h ./kernel/mtype.h ./kernel/big.h ./kernel/mstr.h
	$(CPP) $(CPP_PROJ) $@ $<

$(INTDIR)/parser.obj: ./kernel/parser.c ./kernel/bif.h ./kernel/assert.h 		\
	./kernel/eval.h ./kernel/mm.h ./kernel/encoding.h ./kernel/moi.h 			\
	./kernel/token.h ./kernel/sys_sym.h ./kernel/expr.h ./kernel/sym.h 			\
	./kernel/num.h ./kernel/hash.h ./kernel/sys_msg.h ./kernel/parser.h 		\
	./kernel/mtype.h ./kernel/mstr.h
	$(CPP) $(CPP_PROJ) $@ $<

$(INTDIR)/sym.obj: ./kernel/sym.c ./kernel/bif.h ./kernel/assert.h 				\
	./kernel/sys_sym_dec.inc ./kernel/sys_sym_def.inc ./kernel/mtype.h 			\
	./kernel/encoding.h ./kernel/num.h ./kernel/expr.h ./kernel/sym.h 			\
	./kernel/sys_msg_dec.inc ./kernel/hash.h ./kernel/mm.h ./kernel/mstr.h 
	$(CPP) $(CPP_PROJ) $@ $<

$(INTDIR)/token.obj: ./kernel/token.c ./kernel/hash.h ./kernel/eval.h 			\
	./kernel/mtype.h ./kernel/encoding.h ./kernel/moi.h ./kernel/sys_sym.h 		\
	./kernel/token.h ./kernel/sym.h ./kernel/num.h ./kernel/assert.h 			\
	./kernel/sys_char.h ./kernel/mm.h ./kernel/mstr.h ./kernel/expr.h
	$(CPP) $(CPP_PROJ) $@ $<

$(INTDIR)/kernel.obj: ./kernel/kernel.c ./kernel/kernel.h ./kernel/bif.h 		\
	./kernel/moi.h ./kernel/assert.h ./kernel/eval.h ./kernel/mm.h 				\
	./kernel/encoding.h ./kernel/matcher.h ./kernel/token.h ./kernel/msg.h 		\
	./kernel/sys_sym.h ./kernel/expr.h ./kernel/sym.h ./kernel/num.h 			\
	./kernel/hash.h ./kernel/sys_msg.h ./kernel/parser.h ./kernel/mtype.h 		\
	./kernel/mstr.h
	$(CPP) $(CPP_PROJ) $@ $<

