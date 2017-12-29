#include "log.h"


bool DEBUG_ALL = false;

bool FuncVisit = true;

bool PhiVisit = true;

bool PtrSet = true;

bool EnvDebug = true;

bool LdStVisit = true;

const char *log_level_str[] = { "DEBUG", "INFO", "WARNING", "ERROR" };

enum log_level this_log_level = DEBUG;
