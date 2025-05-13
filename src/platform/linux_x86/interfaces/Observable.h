#pragma once

#ifdef __x86_64__
#include <memory>
#else
#include "shared_ptr.hpp"
#include "weak_ptr.hpp"
#include "make_shared.hpp"
#endif

#include "types.h"
#include "CriticalSection.h"
#include <list>

class CSimpleLock;
class CCriticalSection;

template<typename T>
class IObserverHandle
{
public:
    ~IObserverHandle() {}
    virtual ERROR_CODE_T notifyObserver(T eventInfo) = 0;
};

template<>
class IObserverHandle<void>
{
public:
    ~IObserverHandle() {}
    virtual ERROR_CODE_T notifyObserver(void) = 0;
};

template <class C> class VoidObserverHandleWithObject : public IObserverHandle<void>
{
public:
    VoidObserverHandleWithObject(C *targetObject, ERROR_CODE_T (C::*notify)(void)) : m_pTarget(targetObject), m_notify(notify) {}
    VoidObserverHandleWithObject(shared_ptr<C> targetObject, ERROR_CODE_T (C::*notify)(void)) : m_target(targetObject), m_notify(notify), m_pTarget(NULL) {}

    virtual ERROR_CODE_T notifyObserver(void)
    {
        if (m_pTarget != NULL)
        {
            return (m_pTarget->*m_notify)();
        }
        else
        {
            shared_ptr<C> target = m_target.lock();
            if (target)
            {
                return (target.get()->*m_notify)();
            }
        }

        return STATUS_SUCCESS;
    }

protected:
    C *m_pTarget;
    weak_ptr<C> m_target;
    ERROR_CODE_T (C::*m_notify)(void);
};

template <class C, typename T> class ObserverHandleWithObject : public IObserverHandle<T>
{
public:
    ObserverHandleWithObject(C *targetObject, ERROR_CODE_T (C::*notify)(T)) : m_pTarget(targetObject), m_notify(notify) {}
    ObserverHandleWithObject(shared_ptr<C> targetObject, ERROR_CODE_T (C::*notify)(T)) : m_target(targetObject), m_notify(notify), m_pTarget(NULL) {}
    virtual ERROR_CODE_T notifyObserver(T eventInfo)
    {
        if (m_pTarget != NULL)
        {
            return (m_pTarget->*m_notify)(eventInfo);
        }
        else
        {
            shared_ptr<C> target = m_target.lock();
            if (target)
            {
                return (target.get()->*m_notify)(eventInfo);
            }
        }

        return STATUS_SUCCESS;
    }

protected:
    C *m_pTarget;
    weak_ptr<C> m_target;
    ERROR_CODE_T (C::*m_notify)(T);
};

class VoidObserverHandle : public IObserverHandle<void>
{
public:
    VoidObserverHandle(ERROR_CODE_T (*notify)(void)) : m_notify(notify) {}

    virtual ERROR_CODE_T notifyObserver(void)
    {
        return m_notify();
    }

protected:
    ERROR_CODE_T (*m_notify)(void);
};

template <typename T> class ObserverHandle : public IObserverHandle<T>
{
public:
    ObserverHandle(ERROR_CODE_T (*notify)(T)) : m_notify(notify) {}
    virtual ERROR_CODE_T notifyObserver(T eventInfo)
    {
        return m_notify(eventInfo);
    }

protected:
    ERROR_CODE_T (*m_notify)(T);
};

template <typename T> class Observable
{
public:

    Observable() : concurrent_dispatcher_count(0) {}

    template<class C>
    shared_ptr<IObserverHandle<T> > registerObserver(C *target, ERROR_CODE_T (C::*notify)(T))
    {
        CSimpleLock myLock(&m_cs);
        shared_ptr<IObserverHandle<T> > shared = make_shared<ObserverHandleWithObject<C, T> >(target, notify);
        observers.push_back(shared);
        return shared;
    }

    template<class C>
    shared_ptr<IObserverHandle<T> > registerObserver(shared_ptr<C> target, ERROR_CODE_T (C::*notify)(T))
    {
        CSimpleLock myLock(&m_cs);
        shared_ptr<IObserverHandle<T> > shared = make_shared<ObserverHandleWithObject<C, T> >(target, notify);
        observers.push_back(shared);
        return shared;
    }

    shared_ptr<IObserverHandle<T> > registerObserver(ERROR_CODE_T (*notify)(T))
    {
        CSimpleLock myLock(&m_cs);
        shared_ptr<IObserverHandle<T> > shared = make_shared<ObserverHandle<T> >(notify);
        observers.push_back(shared);
        return shared;
    }

    void notifyObservers(T eventInfo)
    {
        concurrent_dispatcher_count++;
        typename list<weak_ptr<IObserverHandle<T> > >::iterator iter;
        for (iter = observers.begin(); iter != observers.end(); iter++)
        {
            if (shared_ptr<IObserverHandle<T> > observerHandle = iter->lock())
            {
                observerHandle->notifyObserver(eventInfo);
            }
        }
        concurrent_dispatcher_count--;

        // Remove all observers that are gone, only if we are not dispatching.
        if (0 == concurrent_dispatcher_count)
        {
            CSimpleLock myLock(&m_cs);
            for (iter = observers.begin(); iter != observers.end(); )
            {
                if (iter->expired())
                {
                    iter = observers.erase(iter);
                }
                else
                {
                    ++iter;
                }
            }
        }
    }

    INT32U getObserverCount(void)
    {
        // Remove all observers that are gone, only if we are not dispatching.
        if (0 == concurrent_dispatcher_count)
        {
            CSimpleLock myLock(&m_cs);
            typename list<weak_ptr<IObserverHandle<T> > >::iterator iter;
            for (iter = observers.begin(); iter != observers.end(); )
            {
                if (iter->expired())
                {
                    iter = observers.erase(iter);
                }
                else
                {
                    ++iter;
                }
            }
        }

        return observers.size();
    }

protected:
    ICriticalSection m_cs;
    list<weak_ptr<IObserverHandle<T> > > observers;
    INT32U concurrent_dispatcher_count;
};

template <> class Observable<void>
{
    public:

    Observable() : concurrent_dispatcher_count(0) {}

    template<class C>
    shared_ptr<IObserverHandle<void> > registerObserver(C *target, ERROR_CODE_T (C::*notify)(void))
    {
        CSimpleLock myLock(&m_cs);
        shared_ptr<IObserverHandle<void> > shared = make_shared<VoidObserverHandleWithObject<C> >(target, notify);
        observers.push_back(shared);
        return shared;
    }

    template<class C>
    shared_ptr<IObserverHandle<void> > registerObserver(shared_ptr<C> target, ERROR_CODE_T (C::*notify)(void))
    {
        CSimpleLock myLock(&m_cs);
        shared_ptr<IObserverHandle<void> > shared = make_shared<VoidObserverHandleWithObject<C> >(target, notify);
        observers.push_back(shared);
        return shared;
    }

    shared_ptr<IObserverHandle<void> > registerObserver(ERROR_CODE_T (*notify)(void))
    {
        CSimpleLock myLock(&m_cs);
        shared_ptr<IObserverHandle<void> > shared = make_shared<VoidObserverHandle>(notify);
        observers.push_back(shared);
        return shared;
    }

    void notifyObservers(void)
    {
        concurrent_dispatcher_count++;
        list<weak_ptr<IObserverHandle<void> > >::iterator iter;
        for (iter = observers.begin(); iter != observers.end(); iter++)
        {
            if (shared_ptr<IObserverHandle<void> > observerHandle = iter->lock())
            {
                observerHandle->notifyObserver();
            }
        }
        concurrent_dispatcher_count--;

        // Remove all observers that are gone, only if we are not dispatching.
        if (0 == concurrent_dispatcher_count)
        {
            CSimpleLock myLock(&m_cs);
            for (iter = observers.begin(); iter != observers.end(); )
            {
                if (iter->expired())
                {
                    iter = observers.erase(iter);
                }
                else
                {
                    ++iter;
                }
            }
        }
    }

    INT32U getObserverCount(void)
    {
        // Remove all observers that are gone, only if we are not dispatching.
        if (0 == concurrent_dispatcher_count)
        {
            CSimpleLock myLock(&m_cs);
            list<weak_ptr<IObserverHandle<void> > >::iterator iter;
            for (iter = observers.begin(); iter != observers.end(); )
            {
                if (iter->expired())
                {
                    iter = observers.erase(iter);
                }
                else
                {
                    ++iter;
                }
            }
        }

        return observers.size();
    }

protected:
    ICriticalSection m_cs;
    list<weak_ptr<IObserverHandle<void> > > observers;
    INT32U concurrent_dispatcher_count;
};
