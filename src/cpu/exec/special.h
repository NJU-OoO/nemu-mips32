#if 1
make_exec_handler(exec_special) {
  goto *special_table[operands->func];
}

make_exec_handler(exec_special2) {
  goto *special2_table[operands->func];
}

make_exec_handler(exec_special3) {
  goto *special3_table[operands->func];
}

make_exec_handler(exec_bshfl) { goto *bshfl_table[I_SA]; }
make_exec_handler(exec_regimm) { goto *regimm_table[GR_T]; }

make_exec_handler(exec_cop0) {
  if (GR_S & 0x10)
    goto *cop0_table_func[operands->func];
  else
    goto *cop0_table_rs[GR_S];
}

make_exec_handler(exec_cop1) {
  if (GR_S == FPU_FMT_S)
    goto *cop1_table_rs_S[operands->func];
  else if (GR_S == FPU_FMT_D)
    goto *cop1_table_rs_D[operands->func];
  else if (GR_S == FPU_FMT_W)
    goto *cop1_table_rs_W[operands->func];
  else
    goto *cop1_table_rs[GR_S];
}
#endif

make_exec_handler(inv) {
// the pc corresponding to this inst
// pc has been updated by instr_fetch
#if CONFIG_EXCEPTION
  raise_exception(EXC_RI);
#else
  uint32_t instr = vaddr_read(cpu.pc, 4);
  uint8_t *p = (uint8_t *)&instr;
  printf(
      "invalid opcode(pc = 0x%08x): %02x %02x %02x %02x "
      "...\n",
      cpu.pc, p[0], p[1], p[2], p[3]);
  nemu_state = NEMU_END;
#endif
}
