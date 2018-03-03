#include "MapBasedGlobalLockImpl.h"

#include <mutex>
#include <iostream>

namespace Afina {
namespace Backend {

// // See MapBasedGlobalLockImpl.h
    bool MapBasedGlobalLockImpl::Put(const std::string &key, const std::string &value)
    {
        std::unique_lock<std::mutex> guard(_lock);

        auto p = _cacheMap.find(key);               //map iterator
        auto new_size = key.size() + value.size();

        if (p == _cacheMap.end())
        {
            while (_current_size + new_size > _max_size) {
                auto last = _lru.back();

                _current_size -= (int) (last.first.size() + last.second.size());

                _cacheMap.erase(last.first);
                _lru.pop_back();
            }

            _lru.push_front(std::make_pair(key, value));        //put new element at head
            _cacheMap.emplace(_lru.front().first, _lru.begin());
            _current_size += new_size;

        } else {
            _current_size -= p->second->second.size();

            _lru.splice(_lru.begin(), _lru, p->second);
            _lru.front().second = value;

            _current_size += new_size;

        }

        return true;
    }

// See MapBasedGlobalLockImpl.h
    bool MapBasedGlobalLockImpl::PutIfAbsent(const std::string &key, const std::string &value)
    {
        std::unique_lock<std::mutex> guard(_lock);

        auto p = _cacheMap.find(key);               //map iterator
        auto new_size = key.size() + value.size();

        if (p == _cacheMap.end())
        {
            while (_current_size + new_size > _max_size) {
                auto last = _lru.back();

                _current_size -= (int) (last.first.size() + last.second.size());

                _cacheMap.erase(last.first);
                _lru.pop_back();
            }

            _lru.push_front(std::make_pair(key, value));        //put new element at head
            _cacheMap.emplace(_lru.front().first, _lru.begin());
            _current_size += new_size;

        } else {
            return false;
        }

        return true;
    }

// See MapBasedGlobalLockImpl.h
    bool MapBasedGlobalLockImpl::Set(const std::string &key, const std::string &value)
    {
        std::unique_lock<std::mutex> guard(_lock);

        auto p = _cacheMap.find(key); //map iterator
        auto new_size = key.size() + value.size();

        if(p != _cacheMap.end())
        {
            _current_size -= p->second->second.size();

            _lru.splice(_lru.begin(), _lru, p->second);
            _lru.front().second = value;

            _current_size += new_size;
            return true;

        }
        return false;
    }

// See MapBasedGlobalLockImpl.h
    bool MapBasedGlobalLockImpl::Delete(const std::string &key)
    {
        std::unique_lock<std::mutex> guard(_lock);

        auto p = _cacheMap.find(key); //map iterator

        if(p != _cacheMap.end())
        {
            auto item = _cacheMap.find(key);
            _lru.erase(item->second);
            _cacheMap.erase(key);
            return true;
        }
        return false;
    }

// See MapBasedGlobalLockImpl.h
    bool MapBasedGlobalLockImpl::Get(const std::string &key, std::string &value)
    {
        std::unique_lock<std::mutex> guard(_lock);

        auto p = _cacheMap.find(key); //map iterator

        if (p != _cacheMap.end()){
            _lru.splice(_lru.begin(), _lru, p->second);
            value = _lru.front().second;
            return true;
        }
        return false;
    }

// See MapBasedGlobalLockImpl.h
    int MapBasedGlobalLockImpl::GetSize() const {
        return _max_size;
    }


    int MapBasedGlobalLockImpl::GetCurrentSize() const {
        return _current_size;
    }

    bool MapBasedGlobalLockImpl::SetNewCurrentSize(const int newSize) {
        _current_size = newSize;
        return true;
    }




/*
        // See MapBasedGlobalLockImpl.h
        bool MapBasedGlobalLockImpl::Put(const std::string &key, const std::string &value) {
            std::unique_lock<std::mutex> guard(_lock);
            auto new_elem_size = key.size() + value.size();

            if (_cacheMap.find(key) == _cacheMap.end()) {
                if (_current_size + new_elem_size > _max_size) {
                    return false;
                }

                if (_lru.size() + 1 > _max_size) {


                    _cacheMap.erase(_lru.back()); // delete from cache
                    _lru.pop_back();  // erase lru element from the tail
                }
                _lru.push_front(key); //put new element at head
            }

            auto old_elem_it = _cacheMap.find(_lru.back());
            auto old_elem_size = old_elem_it->first.size() + old_elem_it->second.size();
            if (_current_size - old_elem_size + new_elem_size > _max_size) {
                return false;
            }

            _cacheMap[key] = value;
            return true;
        }

// See MapBasedGlobalLockImpl.h
        bool MapBasedGlobalLockImpl::PutIfAbsent(const std::string &key, const std::string &value) {
            std::unique_lock<std::mutex> guard(_lock);
            if (_cacheMap.find(key) == _cacheMap.end()) {
                if (_lru.size() + 1 > _max_size) {
                    _cacheMap.erase(_lru.back());
                    _lru.pop_back();
                }
                _lru.push_front(key);
                _cacheMap[key] = value;
                return true;
            }
            return false;
        }

// See MapBasedGlobalLockImpl.h
        bool MapBasedGlobalLockImpl::Set(const std::string &key, const std::string &value) {
            std::unique_lock<std::mutex> guard(_lock);
            if (_cacheMap.find(key) != _cacheMap.end()) {
                _cacheMap[key] = value;
                return true;
            }
            return false;
        }

// See MapBasedGlobalLockImpl.h
        bool MapBasedGlobalLockImpl::Delete(const std::string &key) {
            std::unique_lock<std::mutex> guard(_lock);
            if (_cacheMap.find(key) != _cacheMap.end()) {
                _cacheMap.erase(key);
                _lru.remove(key);
                return true;
            }
            return false;
        }

// See MapBasedGlobalLockImpl.h
        bool MapBasedGlobalLockImpl::Get(const std::string &key, std::string &value) {
            std::unique_lock<std::mutex> guard(_lock);

            //std::unique_lock<std::mutex> guard(*const_cast<std::mutex *>(&_lock));
            if (_cacheMap.find(key) != _cacheMap.end()) {
                value = _cacheMap.at(key);
                return true;
            }
            return false;
        }

// See MapBasedGlobalLockImpl.h
        size_t MapBasedGlobalLockImpl::GetSize() const {
            return _max_size;
        }


        size_t MapBasedGlobalLockImpl::GetCurrentSize() const {
            return _current_size;
        }

        bool MapBasedGlobalLockImpl::SetNewCurrentSize(const size_t newSize) {
            _current_size = newSize;
            return true;
        }
*/
} // namespace Backend
} // namespace Afina



/*
int main() {
    //Afina::Backend::MapBasedGlobalLockImpl storage = Afina::Backend::MapBasedGlobalLockImpl(2048);
    Afina::Backend::MapBasedGlobalLockImpl storage;
    std::cout << storage.Put("key1", "aaa") << std::endl;
}*/