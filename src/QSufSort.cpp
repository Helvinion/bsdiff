#include "QSufSort.hpp"

static void swap(int64_t& a, int64_t& b)
{
  int64_t tmp = a;
  a = b;
  b = tmp;
}

QSufSort::QSufSort()
  : I (nullptr)
  , V (nullptr)
{
}

QSufSort::~QSufSort()
{
  delete[] I;
  delete[] V;
}

// Put all the occurences of the lowest value of the array on its left side and returns its number of occurence
int64_t QSufSort::sortLowestValue(int64_t* subV, int64_t* subI, int64_t limit)
{
  int64_t x = subV[subI[0]];
  int64_t j = 1;

  for (int64_t i = 1; i < limit; i++)
  {
    int64_t val = subV[subI[i]];

    // We found a new lowest value, so we set the destination of the swap() to come.
    if (val < x)
    {
      x = val;
      j = 0;
    }

    // Swap the value to the next free spot on the left side of the array.
    if (val == x)
    {
      swap(subI[i], subI[j]);
      j++;
    }
  }

  return j;
}

// Has the same result as split(), but more adapted to low size arrays (kind of Select Sort).
void QSufSort::splitEasy(int64_t start, int64_t len, int64_t h)
{
  int64_t  j = 0;
  int64_t* subI = I + start;

  for (int64_t k = 0; k < len; k += j)
  {
    // Put the lowest values of the array on its left.
    j = sortLowestValue(V + h, subI + k, len - k);

    // For each occurence of the lowest value of the array, apply the "unidentified special treatment" 
    for (int64_t i = 0; i < j; i++)
      V[subI[k + i]] = start + k + j - 1;
    if (j == 1)
      subI[k] = -1;
  }
}

QSufSort::PairOfInt QSufSort::countLowerAndEqual(int64_t* subV, int64_t* subI, int64_t len, int64_t x)
{
  PairOfInt ret(0, 0);

  for (int64_t i = 0; i < len; i++)
  {
    if (subV[subI[i]] < x)
      ret.first++;
    if (subV[subI[i]] == x)
      ret.second++;
  }

  return ret;
}

// Range les valeurs de I de sorte à ce que les valeurs inférieures au pivot soient à gauche
// que celles égales soient au milieu, que celles supérieures soient à droite.
void QSufSort::applyPivot(int64_t* subV, int64_t* subI, const PairOfInt& leCounts, int64_t pivot)
{
  int64_t i = 0;
  PairOfInt ehCounts(0, 0);

  // Browse the "lower" part of the array.
  // Put the values "equals" or "higher" in their respective part 
  while (i < leCounts.first)
  {
    if (subV[subI[i]] < pivot)
      i++;
    else if (subV[subI[i]] == pivot)
    {
      swap(subI[i], subI[leCounts.first + ehCounts.first]);
      ehCounts.first++;
    }
    else
    {
      swap(subI[i], subI[leCounts.first + leCounts.second + ehCounts.second]);
      ehCounts.second++;
    }
  }

  // Browse the "equals" part of the array, put the values "higher" in their part.
  int64_t* subSubI = subI + leCounts.first;
  while (ehCounts.first < leCounts.second)
  {
    if (subV[subSubI[ehCounts.first]] == pivot)
      ehCounts.first++;
    else
    {
      swap(subSubI[ehCounts.first], subSubI[leCounts.second + ehCounts.second]);
      ehCounts.second++;
    }
  }

  // The array is now "roughly" sorted value inferor, equal and higher than the pivot
  // are in order, but are not sorted with each other.
}

// This is a kind of Quick Sort with some special treatment
void QSufSort::split(int64_t start, int64_t len, int64_t h)
{
  if (len < 16)
    return splitEasy(start, len, h);

  // Helper variables
  int64_t* subI = I + start;
  int64_t* subV = V + h;

  // Select a pivot
  int64_t  pivot = subV[subI[len / 2]];

  // Get the number of values lower and equal to our pivot.
  PairOfInt leCounts = countLowerAndEqual(subV, subI, len, pivot);

  // Partially sort the array
  applyPivot(subV, subI, leCounts, pivot);

  // Recursion over the lower values
  if (leCounts.first > 0)
    split(start, leCounts.first, h);

  // Unidentified special treatment.
  for (int64_t i = 0; i < leCounts.second; i++)
    V[subI[leCounts.first + i]] = start + leCounts.first + leCounts.second - 1;
  if (leCounts.second == 1)
    subI[leCounts.first] = -1;

  // Recursion over the higher values
  if (len > leCounts.first + leCounts.second)
    split(start + leCounts.first + leCounts.second, len - leCounts.first - leCounts.second, h);
}

void fillCounters(const uint8_t* array, int64_t size, int64_t* combinedCounters)
{
  // Count the number of occurences of each possible byte
  // There are 256 possible bytes.
  // TODO : think about a bucket with 256 * 256 possible values
  int64_t counters[256] = {0};
  for (int64_t i = 0; i < size; i++)
    counters[array[i]]++;

  // Each byte of combinedCounters gives the number of elements in the array
  // that are strictly lower than its index.
  for (int64_t i = 1; i < 256; i++)
    combinedCounters[i] = combinedCounters[i - 1] + counters[i - 1];
}

void fillIandV(const uint8_t* array, int64_t size)
{
  int64_t combinedCounters[256] = {0};
  fillCounters(array, size, combinedCounters);

  // Huge allocation.
  // TODO : think about using int32_t, for memory usage optimisation
  I = new int64_t[size + 1];
  V = new int64_t[size + 1];

  // I contains the index of each byte of the array in order to sort it.
  // V contains the index in I where each byte of the array has been put.
  // It means that V[I[i]] = i, for the moment.
  // combinedCounters is shifted one step to the left in this operation. Trust me.
  // It means that each byte of combinedCounters gives the number of elements in the array
  // that are lower *or equal* than its index.
  V[size] = 0;
  for (int64_t i = 0; i < size; i++)
  {
    uint64_t& index = combinedCounters[array[i]];
    index++;
    I[index] = i;
    V[i] = index;
  }

  // In the (few) cases where a specific byte appears only once,
  // put (-1) at its location in I. We can still get its position in combinedCounters.
  for (int64_t i = 1; i < 256; i++)
  {
    if (combinedCounters[i] - combinedCounters[i - 1] == 1)
      I[combinedCounters[i]] = -1;
  }
  I[0] = -1; // Remember that I[0] is just a placeholder and that it does not contain any index.
}

void sort(const uint8_t* array, int64_t size);
{
  fillIandV(array, size, combinedCounters);

  for (int64_t h = 1; I[0] != -(size + 1); h *= 2)
  {
    // h = 1, 2, 4, 8, 16, 32...
    int64_t len = 0;
    int64_t i = 0;

    while (i < size + 1)
    {
      if (I[i] < 0)
      {
        len += -I[i];
        i += -I[i];
      }
      else
      {
        if (len)
          I[i - len] = -len;
        len = V[I[i]] + 1 - i;
        split(i, len, h);
        i += len;
        len = 0;
      }
    }
    if (len)
      I[i - len] = -len;
  }

  for (i = 0; i < oldsize + 1; i++)
    I[V[i]] = i;
}
