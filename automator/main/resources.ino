#undef __FILENAME__
#define __FILENAME__ "resources"
// Timer table
const PROGMEM uint8_t timertable[24][12] = {
  /*         00        05       10       15       20       25       30       35       40       45       50       55   */
  /* 00 */  { MODEOFF, MODEOFF, MODEOFF, MODEOFF, MODEOFF, MODEOFF, MODEOFF, MODEOFF, MODEOFF, MODEOFF, MODEOFF, MODEOFF },
  /* 01 */  { MODEOFF, MODEOFF, MODEOFF, MODEOFF, MODEOFF, MODEOFF, MODEOFF, MODEOFF, MODEOFF, MODEOFF, MODEOFF, MODEOFF },
  /* 02 */  { MODEOFF, MODEOFF, MODEOFF, MODEOFF, MODEOFF, MODEOFF, MODEOFF, MODEOFF, MODEOFF, MODEOFF, MODEOFF, MODEOFF },
  /* 03 */  { MODEOFF, MODEOFF, MODEOFF, MODEOFF, MODEOFF, MODEOFF, MODEOFF, MODEOFF, MODEOFF, MODEOFF, MODEOFF, MODEOFF },
  /* 04 */  { MODEOFF, MODEOFF, MODEOFF, MODEOFF, MODEOFF, MODEOFF, MODEOFF, MODEOFF, MODEOFF, MODEOFF, MODEOFF, MODEOFF },
  /* 05 */  { MODEOFF, MODEOFF, MODEOFF, MODEOFF, MODEOFF, MODEOFF, MODEOFF, MODEOFF, MODEOFF, MODEOFF, MODEOFF, MODEOFF },
  /* 06 */  { MODEOFF, MODEOFF, MODEOFF, MODEOFF, MODEOFF, MODEOFF, MODEOFF, MODEOFF, MODEOFF, MODEON,  MODEON,  MODEON  },
  /* 07 */  { MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON  },
  /* 08 */  { MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON  },
  /* 09 */  { MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON  },
  /* 10 */  { MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON  },
  /* 11 */  { MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON  },
  /* 12 */  { MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON  },
  /* 13 */  { MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON  },
  /* 14 */  { MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON  },
  /* 15 */  { MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON  },
  /* 16 */  { MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON  },
  /* 17 */  { MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON  },
  /* 18 */  { MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON  },
  /* 19 */  { MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON  },
  /* 20 */  { MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON,  MODEON  },
  /* 21 */  { MODEON,  MODEON,  MODEON,  MODEOFF, MODEOFF, MODEOFF, MODEOFF, MODEOFF, MODEOFF, MODEOFF, MODEOFF, MODEOFF },
  /* 22 */  { MODEOFF, MODEOFF, MODEOFF, MODEOFF, MODEOFF, MODEOFF, MODEOFF, MODEOFF, MODEOFF, MODEOFF, MODEOFF, MODEOFF },
  /* 23 */  { MODEOFF, MODEOFF, MODEOFF, MODEOFF, MODEOFF, MODEOFF, MODEOFF, MODEOFF, MODEOFF, MODEOFF, MODEOFF, MODEOFF }
};

