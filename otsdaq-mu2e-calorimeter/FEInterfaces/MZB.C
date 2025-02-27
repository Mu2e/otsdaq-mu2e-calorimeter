
#define GNU_COMP

#include "otsdaq-mu2e-calorimeter/FEInterfaces/MZB.h"
// #include "otsdaq-mu2e-calorimeter/FEInterfaces/ROCCalorimeterInterface.h"

// #include "otsdaq-mu2e/FEInterfaces/ROCPolarFireCoreInterface.h"

uint8_t* MZB_Encode_CMD_Command_raw(MZB_OSCMDCODE_t cmdCode, float* params) {
	int i;

	// Create structure and fill only to ease the checksum calculation

#ifdef GNU_COMP
	static struct __attribute__((__packed__)) ems {
#else
	__packed __align(4) struct ems {
#endif
		short dummy;
		short adr;

#ifdef GNU_COMP
		struct __attribute__((__packed__)) {
#else
		__packed struct {
#endif
			unsigned short functionReq_tag;  // 'CD'
			unsigned short functionReq;
			float          P[CT_MAXP];

#ifdef GNU_COMP
			// } __attribute__((packed, aligned(4)))
		} __attribute__((aligned(4)))
#else
		}
#endif
		cmdReq;
		unsigned chksum;
#ifdef GNU_COMP
		// } __attribute__((packed, aligned(4)))
	} __attribute__((aligned(4)))
#else
	}
#endif
	em;

	// *** Fill the structure ***

	em.adr = offsetof(EE_DATABUF_t, functionReq_tag);

	// Structure id for the extended command request ('CD')
	em.cmdReq.functionReq_tag = 'C' | ('D' << 8);

	// Command request (enumerated)
	em.cmdReq.functionReq = cmdCode | 0x8000;

	// 9 parameters (float)
	for(i = 0; i < CT_MAXP; i++) {
		em.cmdReq.P[i] = params[i];
	}

	// Checksum
	em.chksum = 1 + (~ees_chksum((void*)&em.cmdReq, sizeof(em.cmdReq)));

	// Print for debug
	int debug = 0;
	if(debug) {
		uint8_t* p = (uint8_t*)&em.adr;
		for(size_t i = 0; i < (sizeof(em) - offsetof(struct ems, adr)); i += 8) {
			for(int j = 0; j < 8; j++) {
				printf("%02x ", *p++);
			}
			printf("\n");
		}
	}

	return (uint8_t*)&em.adr;
}

unsigned ees_chksum(void* ptr, int len) {
	unsigned* src = (unsigned*)ptr;
	unsigned  sum;
	unsigned  tmp;

	for(sum = 0; len >= 4; len -= 4)
		sum += *src++;

	if(len) {
		tmp = 0;
		memcpy(&tmp, src, len);
		sum += tmp;
	}

	return sum;
}

MZB_OSCMDCODE_t mz_string_to_enum(const char* str) {
	for(size_t i = 0; i < sizeof(code_map) / sizeof(code_map[0]); i++) {
		if(code_map[i].str == str) {
			return code_map[i].code;
		}
	}

	return SYNTAX_ERROR;
}
