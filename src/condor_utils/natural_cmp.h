#ifndef __NATURAL_CMP_H__
#define __NATURAL_CMP_H__

// "natural" string compare -- a replacement for strcmp(3)
// takes numeric portions into account, as specified in strverscmp(3)
int natural_cmp(const std::string &str1, const std::string &str2);

#endif
