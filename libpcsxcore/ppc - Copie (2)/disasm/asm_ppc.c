/* radare - LGPL - Copyright 2009-2010 nibble<.ds@gmail.com> */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "dis-asm.h"
#include <xetypes.h>
#include <alloca.h>
#include <malloc.h>

static unsigned long Offset = 0;
static char *buf_global = NULL;
static unsigned char bytes[4];

static int ppc_buffer_read_memory (bfd_vma memaddr, bfd_byte *myaddr, unsigned int length, struct disassemble_info *info) {
	memcpy (myaddr, bytes, length);
	return 0;
}

static int symbol_at_address(bfd_vma addr, struct disassemble_info * info) {
	return 0;
}

static void memory_error_func(int status, bfd_vma memaddr, struct disassemble_info *info) {
	//--
}

static void print_address(bfd_vma address, struct disassemble_info *info) {
	char tmp[32];
	if (buf_global == NULL)
		return;
	sprintf(tmp, "0x%08llx", address);
	strcat(buf_global, tmp);
}

static int buf_fprintf(void *stream, const char *format, ...) {
	va_list ap;
	char *tmp;
	if (buf_global == NULL)
		return 0;
	va_start (ap, format);
 	tmp = malloc (strlen (format)+strlen (buf_global)+2);
	sprintf (tmp, "%s%s", buf_global, format);
	vsprintf (buf_global, tmp, ap);
	va_end (ap);
    free(tmp);
	return 0;
}

int disassemble(u32 a, u32 op) {
	static struct disassemble_info disasm_obj;
	buf_global = calloc(1,1024);
	Offset = a;
	
	printf("%08x:  %08x\t",a,op);
	
	op=__builtin_bswap32(op);
	
	memcpy (bytes, &op, 4); // TODO handle thumb

	/* prepare disassembler */
	memset (&disasm_obj, '\0', sizeof (struct disassemble_info));
	disasm_obj.disassembler_options="64";
	disasm_obj.buffer = bytes;
	disasm_obj.read_memory_func = &ppc_buffer_read_memory;
	disasm_obj.symbol_at_address_func = &symbol_at_address;
	disasm_obj.memory_error_func = &memory_error_func;
	disasm_obj.print_address_func = &print_address;
	disasm_obj.endian = BFD_ENDIAN_UNKNOWN;
	disasm_obj.fprintf_func = &buf_fprintf;
	disasm_obj.stream = stdout;


	int inst_len = print_insn_little_powerpc((bfd_vma)Offset, &disasm_obj);

	if (inst_len == -1)
		strncpy(buf_global, " (data)", 1024);
	
	printf("%s\n",buf_global);
	
	free(buf_global);

	return inst_len;
}