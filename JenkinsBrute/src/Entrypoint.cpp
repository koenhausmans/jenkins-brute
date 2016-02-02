#include <fstream>
#include <unordered_set>
#include <vector>
#include <iterator>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>
#include <algorithm>
#include <thread>
#include <future>
#include <mutex>
#include <sstream>
#include <valarray>

#include <tclap/CmdLine.h>

#include "jenkins.hpp"

using Clock = std::chrono::high_resolution_clock;

class ScopeTimer
{
  public:
    ScopeTimer(std::string scopeName)
        : scopeName_(scopeName), startTime_(Clock::now())
    {
    }

    ~ScopeTimer()
    {
        std::cout << "Scope " << scopeName_ << " finished in "
                  << std::chrono::duration_cast<std::chrono::milliseconds>(
                         Clock::now() - startTime_)
                         .count()
                  << "ms\n";
    }

  private:
    std::string scopeName_;
    Clock::time_point startTime_;
};

#define MakeScopeTimer(scopeName) ScopeTimer timer__scopeName(##scopeName);

std::string GetTimeString()
{
    static Clock::time_point startTime = Clock::now();

    auto timeDifference = std::chrono::duration_cast<std::chrono::milliseconds>(
        Clock::now() - startTime);

    std::string timeString;
    timeString.clear();
    timeString = "[" + std::to_string(timeDifference.count()) + "ms]";

    return timeString;
}

std::unordered_set<uint32_t> hashes;
std::vector<std::string> words;

std::mutex threadLock;

std::ofstream matchedHashesStream;

void TryMatchWord(char const* word, size_t length)
{
    uint32_t hash = jenkins(word, static_cast<int>(length), 0);

    if (hashes.find(hash) != hashes.end())
    {
        std::stringstream ss;
        ss << GetTimeString() << " Found hash: " << std::hex << hash << " = " << word
           << std::dec << "\n";

        {
            std::lock_guard<std::mutex> scopeLock(threadLock);
            std::cout << ss.str();
            matchedHashesStream << ss.str();
        }
    }
}

struct FastString
{
    char buffer[1024]{};
    size_t length = 0;

    inline void Reset()
    {
        length = 0;
    }

    inline void FastAppend(char c)
    {
        buffer[length] = c;
        length += 1;

        NullTerm();
    }

    inline void FastAppend(char* src, size_t len)
    {
        memcpy(buffer + length, src, len);
        length += len;

        NullTerm();
    }

    inline void FastAppend(std::string const& str)
    {
        FastAppend(const_cast<char*>(str.c_str()), str.length());
    }

    inline void NullTerm() { buffer[length] = 0; }
};

struct
{
    bool prependUnderscore = false;
    bool useProperCase = false;
    bool noUnderscores = false;
    int wordDepth = 0;
} Settings;

void advance(std::vector<size_t>& indexes, std::vector<size_t> const& counts)
{
    for (size_t i = counts.size(); i--> 0;)
    {
        indexes[i]++;
        if (indexes[i] < counts[i])
            return;
        indexes[i] = indexes[i] % counts[i];
    }
    // past the end, don't advance:
    indexes = counts;
}

using WordListIterator = std::vector<std::string>::iterator;
void JenkinsBatchTask(size_t taskId, WordListIterator begin, WordListIterator end,
                      std::promise<size_t>&& promise)
{
    {
        std::lock_guard<std::mutex> scopeLock(threadLock);
        std::cout << GetTimeString() << " Starting Task #" << taskId << "\n";
    }

    //std::ptrdiff_t totalWordsForTask = std::distance(begin, end);

    FastString staticBuffer;

    size_t totalWords = 0;

    auto appendWord = [&](std::string const& str)
    {
        if (Settings.prependUnderscore)
            staticBuffer.FastAppend('_');

        staticBuffer.FastAppend(str);
    };
    
    auto startTime = Clock::now();
    for (auto it = begin; it != end; ++it)
    {
        /*auto timeDiff = std::chrono::duration_cast<std::chrono::seconds>(
            Clock::now() - startTime);

        if (timeDiff.count() >= 5)
        {
            std::ptrdiff_t currentProgress = std::distance(begin, it);
            float percentComplete =
                (currentProgress / static_cast<float>(totalWordsForTask)) * 100.f;

            {
                std::lock_guard<std::mutex> scopeLock(threadLock);
                std::cout << GetTimeString() << " Task #" << taskId << " at "
                    << currentProgress << "/" << totalWordsForTask << " ("
                    << percentComplete << ")\n";
            }

            startTime = Clock::now();
        }*/

        staticBuffer.length = 0;
        if (Settings.prependUnderscore)
            staticBuffer.FastAppend('_');

        staticBuffer.FastAppend(*it);
        size_t startPos = staticBuffer.length;

        if (Settings.wordDepth > 1)
        {
            std::vector<size_t> counts;
            for (int i = 0; i < Settings.wordDepth - 1; ++i)
                counts.push_back(words.size());

            for (std::vector<size_t> v(counts.size()); v < counts; advance(v, counts))
            {
                staticBuffer.length = startPos;
                if (!Settings.noUnderscores)
                    staticBuffer.FastAppend('_');

                for (size_t i = 0; i < counts.size(); ++i)
                {
                    staticBuffer.FastAppend(words[v[i]]);
                    if (i + 1 < counts.size())
                    {
                        if (!Settings.noUnderscores)
                            staticBuffer.FastAppend('_');
                    }
                }

                TryMatchWord(staticBuffer.buffer, staticBuffer.length);
                totalWords++;
            }
        }
        else
        {
            TryMatchWord(staticBuffer.buffer, staticBuffer.length);
            totalWords++;
        }
    }

    promise.set_value(totalWords);
}

int main(int argc, char* argv[])
{
    try
    {
        TCLAP::CmdLine cmd("Jenkins BruteForcer");

        TCLAP::SwitchArg argPrependUnderscore("p", "prepend-underscore", "Prepend an underscore to all words");
        cmd.add(argPrependUnderscore);

        TCLAP::SwitchArg argProperCase("c", "proper-case", "Use proper case for words, ex: moo -> Moo");
        cmd.add(argProperCase);

        TCLAP::SwitchArg argsNoUnderscores("u", "no-underscores", "Don't use underscores to separate words");
        cmd.add(argsNoUnderscores);

        TCLAP::ValueArg<int> argWordDepth("d", "word-depth", "How many iterations to expand the word list", false, 2, "number");
        cmd.add(argWordDepth);

        cmd.parse(argc, argv);

        Settings.prependUnderscore = argPrependUnderscore.getValue();
        Settings.wordDepth = argWordDepth.getValue();
        Settings.useProperCase = argProperCase.getValue();
        Settings.noUnderscores = argsNoUnderscores.getValue();
    }
    catch (TCLAP::ArgException const& ex)
    {
        std::cerr << "error: " << ex.error() << " for arg " << ex.argId() << std::endl;
    }

    {
        MakeScopeTimer("Load hashes");

        std::ifstream hashesFileStream("hashes.txt");

        // Imbue the stream with the hex field
        hashesFileStream >> std::hex;

        // Read in the hashes and insert them into the hashes vector
        std::copy(std::istream_iterator<uint32_t>(hashesFileStream),
                  std::istream_iterator<uint32_t>(),
                  std::inserter(hashes, hashes.begin()));
        hashesFileStream.close();

        std::cout << "Loaded " << hashes.size() << " hashes\n";
    }

    {
        MakeScopeTimer("Load words");

        std::ifstream wordFileStream("wordlist.txt");

        std::unordered_set<std::string> uniqueWords;

        // Read in the hashes and insert them into the unique word list
        std::copy(std::istream_iterator<std::string>(wordFileStream),
                  std::istream_iterator<std::string>(),
                  std::inserter(uniqueWords, uniqueWords.begin()));
        wordFileStream.close();

        // Now copy that to our vector for random access
        words.assign(uniqueWords.begin(), uniqueWords.end());

        if (Settings.useProperCase)
        {
            std::transform(words.begin(), words.end(), words.begin(), [](std::string& s) {
                s[0] = static_cast<char>(toupper(s[0]));

                return s;
            });
        }

        std::cout << "Loaded " << words.size() << " words\n";
    }

    matchedHashesStream.open("matched_hashes.txt", std::ios::out | std::ios::trunc);

    // We want to run the brute forcer on all possible threads, except for one. This
    // allows the OS to continue to run without interruption. RIP
    uint32_t concurentThreadsSupported = std::thread::hardware_concurrency() - 1;

    std::cout << GetTimeString() << " Spooling up " << concurentThreadsSupported
              << " threads\n";

    size_t wordsPerThread = words.size() / concurentThreadsSupported;
    size_t wordsLeft = words.size() % concurentThreadsSupported;

    struct Thread
    {
        std::thread thread;
        std::future<size_t> future;
    };

    std::vector<Thread> threadList;

    auto wordListBegin = words.begin();
    for (size_t i = 0; i < concurentThreadsSupported; ++i)
    {
        size_t batchCount = wordsPerThread + (wordsLeft ? 1 : 0);
        if (!batchCount)
            break;

        std::promise<size_t> p;

        Thread t;
        t.future = p.get_future();
        t.thread = std::thread(&JenkinsBatchTask, i, wordListBegin,
                               std::next(wordListBegin, batchCount), std::move(p));

        threadList.emplace_back(std::move(t));
        std::advance(wordListBegin, batchCount);

        if (wordsLeft)
            wordsLeft--;
    }

    size_t totalProcessedHashes = 0;
    for (auto& t : threadList)
    {
        t.thread.join();
        totalProcessedHashes += t.future.get();
    }

    std::cout << GetTimeString() << " processed " << totalProcessedHashes
              << " words\n";
}