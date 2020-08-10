#ifndef SPLITTER_DEFINITIONS_H
#define SPLITTER_DEFINITIONS_H

#include <memory>
#include <vector>
#include <list>
#include <map>
#include <shared_mutex>

typedef std::vector<uint8_t> TFrame;
typedef std::shared_ptr<TFrame> TFramePtr;
typedef std::list<TFramePtr> TFrameBuf;
typedef TFrameBuf::iterator TNextFrame;

typedef std::shared_mutex TLock;
typedef std::unique_lock< TLock >  TWriteLock;
typedef std::shared_lock< TLock >  TReadLock;

#endif /*SPLITTER_DEFINITIONS_H*/
