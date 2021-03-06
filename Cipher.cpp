#include "Cipher.h"
// Cipher.h also gives vector, utility, string

#include <algorithm>
#include <exception>
#include <sstream>

Cipher::Cipher() {
  key_.resize(Cipher::NCARDS);
}

std::vector<int> Cipher::generateKey() {
  int val = 1;
  for (auto i = this->key_.begin(); i != key_.end(); ++i) {
    *i = val++;
  }
  std::random_shuffle(key_.begin(), key_.end());
  // Keep the initial randomly-generated key
  this->initKey_ = key_;
  return key_;
}

std::vector<int> Cipher::generateKeystream(int _n) {
  /* Steps for generating a keystream:
     1. Find JOKER-A (53). Move it down the deck by one step (swap with the one below)
     2. Find JOKER-B (54). Move it down by two steps (swap with adjacent-below twice)
     Note: if "below" is the end of the deck, treat the deck as circular (wrap back to top)
     3. Perform the Triple Cut: swap all cards above higher JOKER with all cards below lower JOKER
     4. Perform the Count Cut: get bottom card (54th card). That gives an index. Either joker is 53, not 54.
        Now search for the card in that position (1-indexed, adjust to 0-indexing when programming) in deck.
        Now all cards up to and including that card, treat it as a slice.
        All cards that are not in that slice, but not including the bottom card (which provided the index!)
        is another slice.
	Swap the two slices.
     5. Find the output card's number. Look at the top card, get its value and use it as an index.
        Using 1-indexing, find the card which the index points to. That card's value is the output!
	If that card is a JOKER, do nothing and start all over again from step 1.
   */
  keystream_.resize(_n);
  int count = 0; int pos; std::pair<int, int> jokers;
  while (count < _n) {
    pos = this->findJokerA();
    this->swapCards(pos, pos + 1);
    pos = this->findJokerB();
    this->swapCards(pos, pos + 1);
    this->swapCards(pos + 1, pos + 2);
    jokers = this->findJokers();
    this->tripleCut(jokers.first, jokers.second);
    pos = key_[Cipher::NCARDS - 1];
    this->countCut(pos); // Handle the JOKER exception inside helper method
    keystream_[count] = this->findOutput(); // Check if std::vector<int> output(_n) was initialized with len 54!
    if (keystream_[count] < 0) {
      continue;
    } else {
      ++count;
    }
  }
  return keystream_;
}

std::string Cipher::encrypt(std::string _plaintext) {
  std::vector<int> textInInts = Cipher::convertCharsToInts(_plaintext);
  int nChars = _plaintext.length();
  key_ = initKey_; // Always reset to the original key before starting encryption
  generateKeystream(nChars);
  // Now add then mod 26
  std::vector<int> ctextInInts(nChars);
  for (int i = 0; i < nChars; ++i) {
    ctextInInts[i] = textInInts[i] + keystream_[i];
    if (ctextInInts[i] > Cipher::RADIX) ctextInInts[i] -= Cipher::RADIX;
  }
  // Translate back to text form by iterating over vector
  return Cipher::convertIntsToChars(ctextInInts);
}

std::string Cipher::decrypt(std::string _ciphertext) {
  std::vector<int> ctextInInts = Cipher::convertCharsToInts(_ciphertext);
  int nChars = _ciphertext.length();
  key_ = initKey_; // Always reset to the original key before starting decryption
  generateKeystream(nChars);
  // Now subtract then mod 26. Remember negatives should wrap around
  std::vector<int> textInInts(nChars);
  for (int i = 0; i < nChars; ++i) {
    textInInts[i] = ctextInInts[i] - keystream_[i];
    if (textInInts[i] < 1) textInInts[i] += Cipher::RADIX;
  }
  // Translate back to text form by iterating over vector
  return Cipher::convertIntsToChars(textInInts);
}

// Private helper methods
std::vector<int> Cipher::convertCharsToInts(std::string _chars) {
  int nChars = _chars.length();
  std::vector<int> output(nChars);
  constexpr char below_a = 'a' - 1;
  for (int i = 0; i < nChars; ++i) {
    output[i] = static_cast<int>(_chars[i] - below_a);
  }
  return output;
}

std::string Cipher::convertIntsToChars(std::vector<int> _ints) {
  std::stringstream outs;
  constexpr char below_a = 'a' - 1;
  for (int i : _ints) {
    // Convert int to char, then stream into outs
    outs << static_cast<char>(i + below_a);
  }
  return outs.str();
}

void Cipher::swapCards(int _pos1, int _pos2) {
  // First, catch the edge-case of the inputs being >= NCARDS
  if (_pos1 > Cipher::NCARDS) _pos1 -= Cipher::NCARDS;
  if (_pos2 > Cipher::NCARDS) _pos2 -= Cipher::NCARDS;
  int temp = key_[_pos2 - 1];
  key_[_pos2 - 1] = key_[_pos1 - 1];
  key_[_pos1 - 1] = temp;
}

int Cipher::findJokerA() const {
  // JokerA's value is 53
  std::vector<int>::const_iterator it = std::find(key_.begin(), key_.end(), 53);
  if (it == key_.end()) throw std::runtime_error("Fatal error: Cipher's key missing JOKERA value");
  return *it;
}

int Cipher::findJokerB() const {
  // JokerB's value is 54
  std::vector<int>::const_iterator it = std::find(key_.begin(), key_.end(), 54);
  if (it == key_.end()) throw std::runtime_error("Fatal error: Cipher's key missing JOKERA value");
  return *it;
}

std::pair<int, int> Cipher::findJokers() const {
  // Jokers are 53 and 54
  std::pair<int, int> output; int numFound = 0;
  for (auto i = key_.begin(); i != key_.end(); ++i) {
    if (*i > 52) {
      // >52 means it is a JOKERA or JOKERB
      if (!numFound) {
	output.first = i - key_.begin() + 1;
        ++numFound;
      } else {
	output.second = i - key_.begin() + 1;
	++numFound;
      }
    }
  }
  if (numFound < 2) throw std::runtime_error("Fatal error: Cipher's key does not contain both JOKERs");
  return output;
}

void Cipher::tripleCut(int _endSlice1, int _startSlice2) {
  // Use std::rotate to perform the tripleCut
  // Suppose I have deck 0 1 2 3 4 5 6. 0 1 2 is first slice, 3 4 is second, 5 6 is third.
  // I want to swap 0 1 2 with 5 6, leaving 3 4 in middle.
  // _endSlice1 position (1-indexed) is 4, which is 3 0-indexed. Notice how it's not inclusive.
  // "first slice is everything above the first Joker"
  // "third slice is everything below the second Joker"
  // First swap: first + second slice with third slice
  // Second swap: second slice with third slice
  int firstPivot = _startSlice2;
  std::rotate(key_.begin(), key_.begin() + firstPivot, key_.end());
  int lenThirdSlice = Cipher::NCARDS - _startSlice2; // This defines the start
  int lenFirstSlice = _endSlice1 - 1; // This defines the pivot point, since it's Third:First:Second now
  std::rotate(key_.begin() + lenThirdSlice, key_.begin() + lenThirdSlice + lenFirstSlice, key_.end());
}

void Cipher::countCut(int _pos) {
  if (_pos > 52) return;
  std::rotate(key_.begin(), key_.begin() + _pos, key_.end() - 1);
  // Use std::rotate
  // e.g. 5 4 3 2 1 0. Last card is 0. Use it to index to first card, value of 5.
  // Now first slice is the card 5. Second slice is 4 3 2 1. Third slice is 0.
  // Swap first and second slices: 4 3 2 1 5 0.
}

int Cipher::findOutput() const {
  int topCardValue = key_[0];
  int output = key_[topCardValue - 1];
  if (output < 0 || output > Cipher::NCARDS)
    throw std::runtime_error("Fatal error: there should not be a card with a negative face value");
  if (output > 52) output = -1;
  if (output > Cipher::RADIX) output -= Cipher::RADIX;
  return output;
}
