#ifdef RTCONFIG_DSL
	#define DSL_N55U 1
#ifdef RTCONFIG_DSL_ANNEX_B
	#define DSL_N55U_ANNEX_B 1
#endif
#else
	#error "no DSL defined, please review makefile"
#endif
