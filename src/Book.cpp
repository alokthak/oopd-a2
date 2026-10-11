#include "bookmgmt/Book.h"
#include "bookmgmt/Pricing.h"

#include <ostream>
#include <stdexcept>

namespace bookmgmt {

std::string joinAuthors(const std::vector<std::string>& authors) {
    std::string out;
    for (std::size_t i = 0; i < authors.size(); ++i) {
        if (i > 0) out += (i + 1 == authors.size()) ? " and " : ", ";
        out += authors[i];
    }
    return out;
}

Book::Book(std::string id, std::string title, std::vector<std::string> authors,
           std::string isbn, std::string publisher, int year, Money unitPrice,
           int edition, Binding binding)
    : Resource(std::move(id), std::move(title), std::move(publisher), year, unitPrice),
      authors_(std::move(authors)),
      isbn_(std::move(isbn)),
      edition_(edition),
      binding_(binding) {
    if (edition_ < 1) throw std::invalid_argument("edition must be >= 1");
}

Money Book::costFor(int copies) const {
    return costAtUnitPrice(copies, unitPrice());
}

Money Book::costAtUnitPrice(int copies, Money price) const {
    requirePositive(copies);
    if (price.isNegative()) throw std::invalid_argument("price must not be negative");
    if (price.currency() != unitPrice().currency())
        throw CurrencyMismatchError("resource and supplied price currencies differ");
    // Apply the surcharge and bulk discount together, rounding only once.
    const int surcharge = binding_ == Binding::Hardcover ? 120 : 100;
    const int discount = copies >= 10 ? 90 : 100;
    return scalePrice(price * copies, surcharge * discount, 10000);
}

void Book::printDetails(std::ostream& os) const {
    os << "  authors: " << joinAuthors(authors_) << "\n"
       << "  isbn: " << isbn_ << "\n"
       << "  edition: " << edition_ << "\n"
       << "  binding: " << (binding_ == Binding::Hardcover ? "Hardcover" : "Paperback")
       << "\n";
}

}  // namespace bookmgmt
