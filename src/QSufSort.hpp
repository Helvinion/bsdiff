#include <cstdint>
#include <utility>

class QSufSort
{
public:
  QSufSort();
  ~QSufSort();

  void sort(const uint8_t* array, int64_t size);
private:
  typedef std::pair<int64_t, int64_t> PairOfInt;

  void      applySpecial(int64_t start, int64_t nLower, int64_t nEqual);
  int64_t   sortLowestValue(int64_t* subV, int64_t* subI, int64_t limit);
  void      splitEasy(int64_t start, int64_t len, int64_t h);
  PairOfInt countLowerAndEqual(int64_t* subV, int64_t* subI, int64_t len, int64_t x);
  void      applyPivot(int64_t* subV, int64_t* subI, const PairOfInt& leCounts, int64_t pivot);
  void      fillIandV(const uint8_t* array, int64_t size);

  void      split(int64_t start, int64_t len, int64_t h);

private:
  int64_t* I;
  int64_t* L;
  int64_t* V;
};

