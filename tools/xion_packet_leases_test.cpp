#include "../src/xion_packet_leases.hpp"
#include <array>
#include <cassert>
#include <iostream>
using kingdom::xion::PacketLeases;
int main() {
    PacketLeases<8> pool;
    int opaque{},translucent{},other{};
    std::array<int,8> next;next.fill(-1);int head=-1;
    for(int n=0;n<8;++n) {
        auto slot=pool.acquire(&opaque,&translucent);assert(slot<8);
        next[slot]=head;head=static_cast<int>(slot);
    }
    // The ninth draw must fall back before touching any linked node.
    assert(pool.acquire(&opaque,&translucent)==pool.unavailable);
    for(int repeat=0;repeat<100;++repeat) {
        int count=0;for(int i=head;i!=-1;i=next[i])assert(++count<=8);
        assert(count==8);assert(pool.acquire(&opaque,&translucent)==pool.unavailable);
    }
    pool.onBufferReset(&other);assert(pool.acquire(&opaque,&translucent)==pool.unavailable);
    pool.onBufferReset(&opaque);assert(pool.acquire(&opaque,&translucent)==pool.unavailable);
    pool.onBufferReset(&translucent);
    for(int n=0;n<8;++n)assert(pool.acquire(&opaque,nullptr)<8);
    assert(pool.acquire(&opaque,nullptr)==pool.unavailable);
    pool.onBufferReset(&opaque);
    const auto canceled=pool.acquire(&opaque,&translucent);assert(canceled<8);pool.cancel(canceled);
    for(int n=0;n<8;++n)assert(pool.acquire(&opaque,&opaque)<8);
    pool.onBufferReset(&opaque);
    for(int n=0;n<8;++n)assert(pool.acquire(&opaque,nullptr)<8);
    assert(pool.acquire(nullptr,nullptr)==pool.unavailable);
    std::cout<<"PASS: queue exhaustion, 100 non-destructive replays, separate buffer retirement, canceled capture, shared buffers\n";
}
