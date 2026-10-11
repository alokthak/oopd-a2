#include "bookmgmt/AudioBook.h"
#include <ostream>
#include <stdexcept>
#include <utility>

namespace bookmgmt {
AudioBook::AudioBook(std::string id, std::string title, std::string publisher, int year,
                     Money pricePerSeat, std::string accessUrl, std::string narrator,
                     int durationMinutes, LicenseModel license, std::optional<Money> platformFee)
    : ElectronicResource(std::move(id), std::move(title), std::move(publisher), year,
                         pricePerSeat, std::move(accessUrl), license, platformFee),
      narrator_(std::move(narrator)), durationMinutes_(durationMinutes) {
    if (durationMinutes_ <= 0)
        throw std::invalid_argument("duration in minutes must be positive");
}
void AudioBook::printDetails(std::ostream& os) const {
    ElectronicResource::printDetails(os);
    os << "  narrator: " << narrator_ << "\n"
       << "  duration in minutes: " << durationMinutes_ << "\n";
}
}  // namespace bookmgmt
