#pragma once

#include <string>
#include <optional>

#include "bookmgmt/Resource.h"

namespace bookmgmt {

enum class LicenseModel { Perpetual, AnnualSubscription };

// An online resource licensed per concurrent-user seat (databases, e-journal
// packages, software, ...). unitPrice = price per seat.
// costFor: platform fee + full price for the first 50 seats,
// then half price for additional seats (pre-tax).
class ElectronicResource : public Resource {
public:
    ElectronicResource(std::string id, std::string title, std::string publisher,
                       int year, Money pricePerSeat, std::string accessUrl,
                       LicenseModel license = LicenseModel::AnnualSubscription,
                       std::optional<Money> platformFee = std::nullopt);

    const std::string& accessUrl() const { return accessUrl_; }
    LicenseModel license() const { return license_; }
    Money platformFee() const { return platformFee_; }

    ResourceCategory category() const override {
        return ResourceCategory::ElectronicResource;
    }
    bool isDigital() const override { return true; }
    Money costFor(int seats) const override;
    Money costAtUnitPrice(int seats, Money price) const override;

protected:
    void printDetails(std::ostream& os) const override;

private:
    std::string accessUrl_;
    LicenseModel license_;
    Money platformFee_;
};

const char* licenseName(LicenseModel m);

}  // namespace bookmgmt
