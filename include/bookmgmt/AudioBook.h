// Roll No.: MT26018
// Name: Alok Kumar Thakur

#pragma once
#include "bookmgmt/ElectronicResource.h"

namespace bookmgmt {
// Model streamed audiobooks as ElectronicResource: access uses licensed seats,
// so digital classification, URL, licence and platform-fee pricing are inherited.
class AudioBook : public ElectronicResource {
public:
    AudioBook(std::string id, std::string title, std::string publisher, int year,
              Money pricePerSeat, std::string accessUrl, std::string narrator,
              int durationMinutes, LicenseModel license = LicenseModel::AnnualSubscription,
              std::optional<Money> platformFee = std::nullopt);
    const std::string& narrator() const { return narrator_; }
    int durationMinutes() const { return durationMinutes_; }
    ResourceCategory category() const override { return ResourceCategory::AudioBook; }
protected:
    void printDetails(std::ostream& os) const override;
private:
    std::string narrator_;
    int durationMinutes_;
};
}  // namespace bookmgmt
