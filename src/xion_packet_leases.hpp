#pragma once
#include <array>
#include <cstddef>

namespace kingdom::xion {
// A J3D packet stays linked after draw(), including presentation replays.
// Only the owning draw buffer's frameInit proves that its link has retired.
// Never recycle a queued packet just because a draw or game tick completed.
template<std::size_t Count> class PacketLeases {
    struct Lease {const void* opaque{}; const void* translucent{};};
    std::array<Lease,Count> leases_{};
    std::size_t next_{};
public:
    static constexpr std::size_t unavailable=Count;
    std::size_t acquire(const void* opaque,const void* translucent) {
        if(!opaque&&!translucent)return unavailable;
        for(std::size_t n=0;n<Count;++n) {
            const auto index=(next_+n)%Count;
            auto& lease=leases_[index];
            if(!lease.opaque&&!lease.translucent) {
                lease={opaque,translucent};next_=(index+1)%Count;return index;
            }
        }
        return unavailable;
    }
    // Only for capture failure before the packet has been submitted.
    void cancel(std::size_t index) {if(index<Count)leases_[index]={};}
    void onBufferReset(const void* buffer) {
        if(!buffer)return;
        for(auto& lease:leases_) {
            if(lease.opaque==buffer)lease.opaque=nullptr;
            if(lease.translucent==buffer)lease.translucent=nullptr;
        }
    }
};
} // namespace kingdom::xion
