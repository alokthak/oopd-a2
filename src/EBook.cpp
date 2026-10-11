// Roll No.: MT26018
// Name: Alok Kumar Thakur

#include "bookmgmt/EBook.h"
#include "bookmgmt/Book.h"
#include <ostream>
#include <stdexcept>
#include <utility>

namespace bookmgmt {
const char* fileFormatName(FileFormat format) {
    switch (format) {
        case FileFormat::PDF: return "PDF";
        case FileFormat::EPUB: return "EPUB";
        case FileFormat::HTML: return "HTML";
    }
    throw std::invalid_argument("unsupported e-book file format");
}
EBook::EBook(std::string id, std::string title, std::vector<std::string> authors,
             std::string isbn, std::string publisher, int year, Money pricePerSeat,
             std::string accessUrl, FileFormat format, bool drmProtected,
             LicenseModel license, std::optional<Money> platformFee)
    : ElectronicResource(std::move(id), std::move(title), std::move(publisher), year,
                         pricePerSeat, std::move(accessUrl), license, platformFee),
      authors_(std::move(authors)), isbn_(std::move(isbn)), format_(format),
      drmProtected_(drmProtected) {
    (void)fileFormatName(format_);
}
void EBook::printDetails(std::ostream& os) const {
    ElectronicResource::printDetails(os);
    os << "  authors: " << joinAuthors(authors_) << "\n"
       << "  isbn: " << isbn_ << "\n"
       << "  file format: " << fileFormatName(format_) << "\n"
       << "  drm protected: " << (drmProtected_ ? "yes" : "no") << "\n";
}
}  // namespace bookmgmt
