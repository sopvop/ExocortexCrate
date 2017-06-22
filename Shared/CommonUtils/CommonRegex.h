#ifndef __COMMON_REGEX_H
#define __COMMON_REGEX_H

#include <memory>
#include <string>

namespace SearchReplace {
class NoReplace  // serve as a base class
{
 public:
  virtual std::string replace(const std::string &str) const;
};

typedef std::shared_ptr<NoReplace> ReplacePtr;

ReplacePtr createReplacer(const std::string *exp = 0,
                          const std::string *form = 0);
ReplacePtr createReplacer(const std::string &exp, const std::string &form);
}

namespace EnvVariables {
std::string replace(const std::string &str);
}

#endif
