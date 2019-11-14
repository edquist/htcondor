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

    JOB_ID_KEY operator+(int i) const { return {cluster, proc + i}; }
    JOB_ID_KEY operator-(int i) const { return {cluster, proc - i}; }

    JOB_ID_KEY &operator++() { ++proc; return *this; }
    JOB_ID_KEY &operator--() { --proc; return *this; }

    JOB_ID_KEY() : cluster(0), proc(0) {}
    JOB_ID_KEY(int c, int p) : cluster(c), proc(p) {}


    bool operator==(const JOB_ID_KEY &b) const
    { return cluster == b.cluster && proc == b.proc; }

    bool operator!=(const JOB_ID_KEY &b) const { return !(*this == b); }
    bool operator<=(const JOB_ID_KEY &b) const { return !(b < *this);  }
};

