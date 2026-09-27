/**
  * Assignment: synchronization
  * Operating Systems
  */

/**
  Hint: F2 (or Control-klik) on a functionname to jump to the definition
  Hint: Ctrl-space to auto complete a functionname/variable.
  */

// function/class definitions you are going to use
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <mutex>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <shared_mutex>
#include <atomic>
#include <regex>



// although it is good habit, you don't have to type '' before many objects by including this line
using namespace std;

// MOD: implementation of the logger
class Logger {

public:
    // MOD: Using flexible size
    using size_type = vector<string>::size_type;

    // MOD: Takes the unique write lock and appends to the vector
    void write(string line) {
        // Critical section begin for logger state - no readers/writers allowed, protects log_
        unique_lock<rw_mutex_t> lk(rw_);
        log_.push_back(move(line));
        // Critical section end for logger state
    }

    // MOD: Takes the shared read lock and returns a copy of the entry
    string read(size_type idx) const {
        // Critical section begin for logger state - many readers allowed, excludes writers, protects log_
        shared_lock<rw_mutex_t> lk(rw_);
        return log_.at(idx);
        // Critical section end for logger state
    }

    // MOD: Takes shared lock and returns a copy of the entire log, safe for multiple readers
    vector<string> read_all() const {
        // Critical section begin for logger state - many readers allowed, excludes writers, protects log_
        shared_lock<rw_mutex_t> lk(rw_);
        return log_;
        // Critical section end for logger state
    }

    // MOD: Returns the current number of log entries
    size_type size() const {
        // Critical section begin for logger state - many readers allowed, excludes writers, protects log_
        shared_lock<rw_mutex_t> lk(rw_);
        return log_.size();
        // Critical section end for logger state
    }

private:
    using rw_mutex_t = shared_timed_mutex;
    mutable rw_mutex_t rw_;
    vector<string> log_;
};

// MOD: Implementation of the buffer
class Buffer {
public:
    // MOD: Constructor for the buffer with name, log, bound, size
    Buffer(string name, shared_ptr<Logger> logger, bool bounded=false, size_t bound=0)
        : name_(move(name)), logger_(move(logger)),
          bounded_(bounded), bound_(bound), seq_(0) {}

    // MOD: Add int to the back of the buffer
    bool add(int value) {
        bool ok = false;
        uint64_t myseq = 0;
        size_t new_size = 0;

        {   // MOD: Ensure lock
            // Critical section begin for buffer state, protects buf_, bounded_, bound_, seq_
            lock_guard<mutex> lk(m_);
            // MOD: Check bound
            if (!bounded_ || buf_.size() < bound_) {
                buf_.push_back(value);
                ok = true;
            }
            // MOD: Increment size of buffer
            new_size = buf_.size();
            myseq = ++seq_;
            // Critical section end for buffer state
        }
        // MOD: Log the change in the logger
        log_add(value, ok, myseq, new_size);
        return ok;
    }

    // MOD: Remove an integer from the front of the buffer
    pair<bool,int> remove() {
        bool ok = false;
        int out = 0;
        uint64_t myseq = 0;
        size_t new_size = 0;

        {
            // MOD: Ensure lock just as by add()
            // Critical section begin for buffer state, protects buf_, bounded_, bound_, seq_
            lock_guard<mutex> lk(m_);
            if (!buf_.empty()) {
                out = buf_.front();
                buf_.erase(buf_.begin());
                ok = true;
            }
            // MOD: Adapt buffer size
            new_size = buf_.size();
            myseq = ++seq_;
            // Critical section end for buffer state
        }
        // MOD: Log changes
        log_remove(ok, out, myseq, new_size);
        return {ok, out};
    }

    // MOD: Change the bound of the buffer
    // MOD: Does not trim records if the existing buffer is already larger, instead, waits until size drops below bound
    void set_bound(size_t new_bound) {
        uint64_t myseq = 0;
        size_t sz = 0;

        {
            // MOD: Grab mutex lock
            // Critical section begin for buffer state, protects buf_, bounded_, bound_, seq_
            lock_guard<mutex> lk(m_);
            bound_ = new_bound;
            bounded_ = true;
            sz = buf_.size();
            myseq = ++seq_;
            // Critical section end for buffer state
        }
        // MOD: Log changes
        log_generic("set_bound", true, myseq, sz, "bound=" + to_string(new_bound));
    }

    // MOD: Make buffer size unbounded
    void make_unbounded() {
        uint64_t myseq = 0;
        size_t sz = 0;
        {
            // MOD: Grab lock
            // Critical section begin for buffer state, protects buf_, bounded_, seq_
            lock_guard<mutex> lk(m_);
            bounded_ = false;
            sz = buf_.size();
            myseq = ++seq_;
            // Critical section end for buffer state
        }
        // MOD: Log changes
        log_generic("make_unbounded", true, myseq, sz, "");
    }

    // MOD: Returns size for tests, not logged
    size_t size() const {
        // Critical section begin for buffer state, protects buf_
        lock_guard<mutex> lk(m_);
        return buf_.size();
        // Critical section end for buffer state
    }

    // MOD: Accessor for name of the buffer
    string name() const { return name_; }

private:
    // MOD: Different helper functions for testing
    void log_add(int v, bool ok, uint64_t seq, size_t sz_after) {
        ostringstream oss;
        oss << "[buffer=" << name_ << " seq=" << seq << "] add(" << v << ") "
            << (ok ? "SUCCESS" : "FAIL") << " size=" << sz_after
            << (bounded_ ? " (bounded:" + to_string(bound_) + ")" : " (unbounded)");
        logger_->write(oss.str());
    }

    void log_remove(bool ok, int v, uint64_t seq, size_t sz_after) {
        ostringstream oss;
        oss << "[buffer=" << name_ << " seq=" << seq << "] remove() "
            << (ok ? "SUCCESS value=" + to_string(v) : "FAIL (empty)")
            << " size=" << sz_after
            << (bounded_ ? " (bounded:" + to_string(bound_) + ")" : " (unbounded)");
        logger_->write(oss.str());
    }

    void log_generic(const string& op, bool ok, uint64_t seq,
                     size_t sz_after, const string& extra) {
        ostringstream oss;
        oss << "[buffer=" << name_ << " seq=" << seq << "] " << op << "() "
            << (ok ? "SUCCESS" : "FAIL")
            << " size=" << sz_after
            << (extra.empty() ? "" : " " + extra)
            << (bounded_ ? " (bounded:" + to_string(bound_) + ")" : " (unbounded)");
        logger_->write(oss.str());
    }

private:
    // MOD: Initialise name of the buffer, logger, mutex lock, buffer, size, boundedness, and integer that we add
    string name_;
    shared_ptr<Logger> logger_;

    mutable mutex m_;
    vector<int> buf_;

    bool bounded_{false};
    size_t bound_{0};

    uint64_t seq_;
};

// MOD: Testing
struct TestResult {
    string name;
    int passed = 0;
    int failed = 0;
    vector<string> notes;
};
static void note(TestResult& tr, const string& s){ tr.notes.push_back(s); }
static void expect(TestResult& tr, bool cond, const string& msg){
    (cond ? ++tr.passed : ++tr.failed);
    if(!cond) tr.notes.push_back("FAIL: " + msg);
}

// MOD: Helpers to analyze logs
static size_t count_ops(const vector<string>& all, const string& buf) {
    return count_if(all.begin(), all.end(),
        [&](const string& s){ return s.find("[buffer=" + buf + " ") != string::npos; });
}
static size_t max_seq(const vector<string>& all, const string& buf) {
    regex re("\\[buffer=" + buf + " seq=(\\d+)\\]");
    smatch m;
    size_t mx = 0;
    for (auto& s : all) if (regex_search(s, m, re)) mx = max(mx, static_cast<size_t>(stoul(m[1])));
    return mx;
}
static bool seq_is_contiguous_1N(const vector<string>& all, const string& buf) {
    regex re("\\[buffer=" + buf + " seq=(\\d+)\\]");
    smatch m;
    vector<size_t> seqs;
    for (auto& s : all) if (regex_search(s, m, re)) seqs.push_back(stoul(m[1]));
    sort(seqs.begin(), seqs.end());
    for (size_t i=0;i<seqs.size();++i) if (seqs[i] != i+1) return false;
    return true;
}
static size_t count_contains(const vector<string>& all, const string& token){
    return count_if(all.begin(), all.end(), [&](const string& s){ return s.find(token)!=string::npos; });
}

// MOD: T1 - Testing single threaded functional correctness
static TestResult T1_single_functional() {
    TestResult tr{"T1: Single-thread functional correctness"};
    auto logger = make_shared<Logger>();
    Buffer B{"B", logger, /*bounded=*/false};

    auto r0 = B.remove();
    expect(tr, !r0.first, "remove() on empty should fail");

    expect(tr, B.add(1), "add(1) should succeed");
    expect(tr, B.add(2), "add(2) should succeed");
    expect(tr, B.add(3), "add(3) should succeed");

    auto r1 = B.remove(); expect(tr, r1.first && r1.second==1, "first remove returns 1");
    auto r2 = B.remove(); expect(tr, r2.first && r2.second==2, "second remove returns 2");

    expect(tr, B.size()==1, "final size should be 1");

    auto all = logger->read_all();
    expect(tr, all.size() == 6, "log should have 6 lines (1 fail + 3 adds + 2 removes)");

    return tr;
}

// MOD: T2 - Testing bounded capacity
static TestResult T2_bounded_capacity() {
    TestResult tr{"T2: Bounded capacity enforcement"};
    auto logger = make_shared<Logger>();
    Buffer B{"B", logger, /*bounded=*/true, /*bound=*/3};

    expect(tr, B.add(10), "add #1");
    expect(tr, B.add(20), "add #2");
    expect(tr, B.add(30), "add #3");
    bool ok4 = B.add(40);
    expect(tr, !ok4, "add past bound should fail");
    auto all = logger->read_all();
    // MOD: Fail on last line
    expect(tr, all.back().find("add(40) ") != string::npos &&
               all.back().find("FAIL") != string::npos,
           "fourth add is logged as FAIL");
    return tr;
}

// MOD: T3 - Switch policies at runtime
static TestResult T3_switch_policies() {
    TestResult tr{"T3: Runtime policy switch"};
    auto logger = make_shared<Logger>();
    Buffer B{"B", logger, /*bounded=*/false};

    for(int i=0;i<5;++i) expect(tr, B.add(100+i), "initial add");

    B.set_bound(3);
    bool add_fail = !B.add(999);
    expect(tr, add_fail, "add while size>=bound should fail");

    for(int i=0;i<3;++i){ auto r=B.remove(); (void)r; }
    expect(tr, B.add(777), "add after size<bound should succeed");

    B.make_unbounded();
    expect(tr, B.add(888), "add in unbounded mode succeeds");

    auto all = logger->read_all();
    expect(tr, count_contains(all, "set_bound() SUCCESS")>=1, "set_bound logged");
    expect(tr, count_contains(all, "make_unbounded() SUCCESS")>=1, "make_unbounded logged");
    return tr;
}

// MOD: T4 - Concurrency: 1 producer and 1 consumer
static TestResult T4_pc_same_buffer() {
    TestResult tr{"T4: 1P/1C same buffer"};
    auto logger = make_shared<Logger>();
    Buffer B{"B", logger, /*bounded=*/true, /*bound=*/5};

    thread prod([&]{ for(int i=0;i<50;++i){ B.add(i); this_thread::sleep_for(chrono::milliseconds(1)); }});
    thread cons([&]{ for(int i=0;i<50;++i){ (void)B.remove(); this_thread::sleep_for(chrono::milliseconds(1)); }});
    prod.join(); cons.join();

    auto all = logger->read_all();

    expect(tr, count_contains(all, "add(") > 0, "some adds exist");
    expect(tr, count_contains(all, "remove() ") > 0, "some removes exist");

    expect(tr, B.size() <= 5, "final size respects bound");
    return tr;
}


// MOD: T5 - Multiple buffers share one logger (interleaving)
static TestResult T5_shared_logger_interleave() {
    TestResult tr{"T5: Shared logger interleaving"};
    auto logger = make_shared<Logger>();
    Buffer X{"X", logger, false}, Y{"Y", logger, true, 4};

    thread a([&]{ for(int i=0;i<30;++i){ X.add(i); (void)Y.remove(); this_thread::sleep_for(chrono::milliseconds(1)); } });
    thread b([&]{ for(int i=0;i<30;++i){ (void)X.remove(); Y.add(i); this_thread::sleep_for(chrono::milliseconds(1)); } });
    a.join(); b.join();

    auto all = logger->read_all();
    expect(tr, count_ops(all,"X")>0 && count_ops(all,"Y")>0, "both buffers logged");

    int distinct=0;
    for(size_t i=0;i<min<size_t>(10, all.size()); ++i)
        if(all[i].find("[buffer=X ")!=string::npos || all[i].find("[buffer=Y ")!=string::npos) ++distinct;
    expect(tr, distinct>=2, "early log shows interleaving");
    return tr;
}

// MOD: T6 - Logger concurrent reads
static TestResult T6_logger_concurrent_reads() {
    TestResult tr{"T6: Logger concurrent reads"};
    auto logger = make_shared<Logger>();
    Buffer B{"B", logger, false};

    atomic<bool> stop{false};
    thread reader1([&]{ while(!stop){ (void)logger->size(); this_thread::sleep_for(chrono::milliseconds(1)); }});
    thread reader2([&]{ while(!stop){ auto snap=logger->read_all(); (void)snap.size(); this_thread::sleep_for(chrono::milliseconds(1)); }});

    thread writer([&]{ for(int i=0;i<200;++i){ if(i%2==0) B.add(i); else (void)B.remove();
                                this_thread::sleep_for(chrono::milliseconds(1)); } stop=true; });

    writer.join(); reader1.join(); reader2.join();
    auto all = logger->read_all();
    expect(tr, all.size()>=100, "many writes succeeded while readers ran");
    return tr;
}

// MOD: T7 — Sequence linearization per buffer
static TestResult T7_sequence_linearization() {
    TestResult tr{"T7: Sequence linearization"};
    auto logger = make_shared<Logger>();
    Buffer B{"B", logger, false};
    for(int i=0;i<50;++i){ if(i%3) B.add(i); else (void)B.remove(); }
    auto all = logger->read_all();
    size_t ops = count_ops(all,"B");
    expect(tr, seq_is_contiguous_1N(all,"B"), "seq 1..N contiguous");
    expect(tr, max_seq(all,"B")==ops, "max seq equals number of ops");
    return tr;
}

// T8 — Trying to force empty/full buffers
static TestResult T8_failure_semantics() {
    TestResult tr{"T8: Failure semantics"};
    auto logger = make_shared<Logger>();
    Buffer B1{"B1", logger, true, 2};
    Buffer B2{"B2", logger, false};

    (void)B1.remove();
    B1.add(1); B1.add(2);
    bool f = !B1.add(3);
    expect(tr, f, "bounded add fails at capacity");

    (void)B2.remove();
    B2.add(1); (void)B2.remove(); (void)B2.remove();

    auto all = logger->read_all();
    size_t rem_fail = count_contains(all, "remove() FAIL");
    size_t add_fail = count_contains(all, " add(") - count_contains(all, " add(") + (count_contains(all, " add(")); // trivial use; keep only checks below

    expect(tr, count_contains(all, "remove() FAIL")>=2, "multiple remove fails on empty observed");
    size_t add_fail_lines = 0;
    for(auto& s: all) if(s.find(" add(")!=string::npos && s.find("FAIL")!=string::npos) ++add_fail_lines;
    expect(tr, add_fail_lines>=1, "at least one add FAIL present");
    return tr;
}

// MOD: T9 - Logger read is non-destructive
static TestResult T9_logger_non_destructive() {
    TestResult tr{"T9: Logger read non-destructive"};
    auto logger = make_shared<Logger>();
    Buffer B{"B", logger, false};
    for(int i=0;i<20;++i) B.add(i);
    auto s1 = logger->size();
    auto e0 = logger->read(0);
    auto snap = logger->read_all();
    auto s2 = logger->size();
    expect(tr, s2==s1, "size unchanged after reads");
    expect(tr, !snap.empty() && !e0.empty(), "entries readable");
    return tr;
}


// MOD: Testing in main
int main() {
    vector<TestResult> results;

    results.push_back(T1_single_functional());
    results.push_back(T2_bounded_capacity());
    results.push_back(T3_switch_policies());
    results.push_back(T4_pc_same_buffer());
    results.push_back(T5_shared_logger_interleave());
    results.push_back(T6_logger_concurrent_reads());
    results.push_back(T7_sequence_linearization());
    results.push_back(T8_failure_semantics());
    results.push_back(T9_logger_non_destructive());

    // Print summary
    cout << "\nSummary: \n";
    int totalPass=0, totalFail=0;
    for (auto& tr : results) {
        cout << tr.name << ": " << tr.passed << " passed, " << tr.failed << " failed\n";
        totalPass += tr.passed; totalFail += tr.failed;
        for (auto& n : tr.notes) cout << "  - " << n << "\n";
    }
    cout << "TOTAL: " << totalPass << " passed, " << totalFail << " failed\n";
    return (totalFail==0 ? 0 : 1);
}

