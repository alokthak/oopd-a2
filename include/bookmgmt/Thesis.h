#pragma once
#include "bookmgmt/Resource.h"

namespace bookmgmt {
// Model a deposited print thesis directly as Resource: it needs neither Book's
// ISBN/binding metadata nor electronic licensing. Default price is zero, but
// paid copies are supported through Resource's per-copy pricing.
class Thesis : public Resource {
public:
    Thesis(std::string id, std::string title, std::string university,
           std::string degree, std::string supervisor, int year, Money unitPrice = Money{});
    const std::string& university() const { return university_; }
    const std::string& degree() const { return degree_; }
    const std::string& supervisor() const { return supervisor_; }
    ResourceCategory category() const override { return ResourceCategory::Thesis; }
protected:
    void printDetails(std::ostream& os) const override;
private:
    std::string university_;
    std::string degree_;
    std::string supervisor_;
};
}  // namespace bookmgmt
