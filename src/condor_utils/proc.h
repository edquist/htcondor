#pragma once

struct JOB_ID_KEY {
    int cluster;
    int proc;

    // a LessThan operator suitable for inserting into a sorted map or set
    bool operator<(const JOB_ID_KEY& cp) const {
        int diff = this->cluster - cp.cluster;
        if (!diff) diff = this->proc - cp.proc;
        return diff < 0;
    }

    JOB_ID_KEY() : cluster(0), proc(0) {}
    JOB_ID_KEY(int c, int p) : cluster(c), proc(p) {}
};

inline bool operator==( const JOB_ID_KEY a, const JOB_ID_KEY b)
{ return a.cluster == b.cluster && a.proc == b.proc; }

