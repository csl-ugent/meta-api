#include <QString>
#include <core/Group.h>

__attribute__((used))
QString *__DIABLO_META_API__constructor_QString(const char *x) {
  return new QString(x);
}

__attribute((used))
void __DIABLO_META_API__destructor_Group(Group *g) {
  delete g;
}
