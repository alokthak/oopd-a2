#include "bookmgmt/ElectronicResource.h"
#include "bookmgmt/Pricing.h"

#include <ostream>
#include <stdexcept>

namespace bookmgmt {

const char* licenseName(LicenseModel m) {
    switch (m) {
        case LicenseModel::Perpetual: return "Perpetual";
        case LicenseModel::AnnualSubscription: return "Annual subscription";
    }
    return "Unknown";
}

ElectronicResource::ElectronicResource(std::string id, std::string title,
                                       std::string publisher, int year,
                                       Money pricePerSeat, std::string accessUrl,
                                       LicenseModel license, std::optional<Money> platformFee)
    : Resource(std::move(id), std::move(title), std::move(publisher), year, pricePerSeat),
      accessUrl_(std::move(accessUrl)),
      license_(license),
      platformFee_(platformFee.value_or(Money::fromMinor(0, pricePerSeat.currency()))) {
    if (platformFee_.currency() != pricePerSeat.currency())
        throw CurrencyMismatchError("platform fee and unit price currencies differ");
    if (platformFee_.isNegative())
        throw std::invalid_argument("platform fee must not be negative");
}

Money ElectronicResource::costFor(int seats) const {
    return costAtUnitPrice(seats, unitPrice());
}

Money ElectronicResource::costAtUnitPrice(int seats, Money price) const {
    requirePositive(seats);
    if (price.isNegative()) throw std::invalid_argument("price must not be negative");
    if (price.currency() != unitPrice().currency())
        throw CurrencyMismatchError("resource and supplied price currencies differ");
    const int fullPriceSeats = seats > 50 ? 50 : seats;
    const int halfPriceSeats = seats > 50 ? seats - 50 : 0;
    return platformFee_ + price * fullPriceSeats
           + scalePrice(price * halfPriceSeats, 1, 2);
}

void ElectronicResource::printDetails(std::ostream& os) const {
    os << "  access url: " << accessUrl_ << "\n"
       << "  license: " << licenseName(license_) << "\n"
       << "  platform fee: " << platformFee_ << "\n";
}

}  // namespace bookmgmt
