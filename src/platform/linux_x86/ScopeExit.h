#ifndef SCOPEEXIT_H
#define SCOPEEXIT_H

template <typename F>
class CleanerScopeExit {
public:
    explicit CleanerScopeExit(F f) : func(std::move(f)), active(true) {}
    ~CleanerScopeExit() { if (active) func(); }
    void Dismiss() { active = false; }

private:
    F func;
    bool active;
};

template <typename F>
CleanerScopeExit<F> MakeScopeExit(F f) {
    return CleanerScopeExit<F>(f);
}


#define ScopeExit(x, y, drop, z) \
    auto x = MakeScopeExit([z] { \
        y; \
    }); 


#endif

