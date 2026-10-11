// Minimal self-contained test runner (no external framework needed).

// Prevent a direct build with NDEBUG from silently skipping the tests.
#ifdef NDEBUG
#error "Assertions must be enabled for this test executable (compile with -UNDEBUG)."
#endif
#include <cassert>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>

#include "bookmgmt/bookmgmt.h"

using namespace bookmgmt;

// Return false for no exception or the wrong type, preserving CHECK_THROWS semantics.
template <typename Exception, typename Function>
bool throwsExpected(Function&& action) {
    try {
        action();
    } catch (const Exception&) {
        return true;
    } catch (...) {
        return false;
    }
    return false;
}

static void testMoney() {
    assert(Money::of(12, 5).toString() == "12.05");
    assert(Money::of(-3, 50).toString() == "-3.50");
    assert(Money::fromMinor(7).toString() == "0.07");
    assert(Money::of(10) + Money::of(0, 50) == Money::fromMinor(1050));
    assert(Money::of(3) * 4 == Money::of(12));
    assert(Money::of(1) < Money::of(2));
    assert((throwsExpected<std::invalid_argument>([&] { (void)(Money::of(1, 100)); })));
}

static void testResourcesAndCost() {
    Book b("B1", "T", {"A", "B", "C"}, "isbn", "P", 2020, Money::of(100));
    assert(b.category() == ResourceCategory::Book);
    assert(!b.isDigital());
    assert(b.costFor(3) == Money::of(300));
    assert((throwsExpected<std::invalid_argument>([&] { (void)(b.costFor(0)); })));
    assert(joinAuthors(b.authors()) == "A, B and C");

    ElectronicResource e("R1", "DB", "P", 2026, Money::of(10), "url",
                         LicenseModel::AnnualSubscription, Money::of(100));
    assert(e.isDigital());
    assert(e.costFor(5) == Money::of(150));

    assert(e.category() == ResourceCategory::ElectronicResource);

    // Polymorphism through a base-class reference
    const Resource& r = e;
    assert(r.costFor(1) == Money::of(110));
    std::ostringstream os;
    os << r;
    assert(os.str().find("platform fee: 100.00") != std::string::npos);

    assert((throwsExpected<std::invalid_argument>([&] { (void)(Book("", "T", {}, "", "", 2000, Money::of(1))); })));
    assert((throwsExpected<std::invalid_argument>([&] { (void)(Book("B", "T", {}, "", "", 2000, Money::fromMinor(-1))); })));
}

static void testCatalog() {
    Catalog c;
    c.emplace<Book>("B1", "Clean Code", std::vector<std::string>{"M"}, "i", "P", 2008,
                    Money::of(1));
    c.emplace<Book>("B2", "Clean Architecture", std::vector<std::string>{"M"}, "i", "P",
                    2017, Money::of(1));
    c.emplace<ElectronicResource>("R1", "ACM Digital Library", "ACM", 2026, Money::of(1),
                                  "url");

    assert(c.size() == 3);
    assert(c.contains("B1"));
    assert(c.find("nope") == nullptr);
    assert((throwsExpected<NotFoundError>([&] { (void)(c.get("nope")); })));
    assert((throwsExpected<DuplicateIdError>([&] { (void)(c.emplace<Book>("B1", "dup", std::vector<std::string>{}, "", "", 1,
                                 Money::of(1))); })));

    assert(c.searchTitle("clean").size() == 2);
    assert(c.byCategory(ResourceCategory::ElectronicResource).size() == 1);
    assert(c.where([](const Resource& r) { return r.isDigital(); }).size() == 1);

    assert(c.holdings("B1") == 0);
    c.addHoldings("B1", 3);
    assert(c.holdings("B1") == 3);
    assert((throwsExpected<std::invalid_argument>([&] { (void)(c.addHoldings("B1", -5)); })));

    c.remove("R1");
    assert(c.size() == 2);
    assert((throwsExpected<NotFoundError>([&] { (void)(c.remove("R1")); })));
}

static void testBudget() {
    Budget b(Money::of(1000));
    b.setQuota(ResourceCategory::Book, {5, Money::of(400)});

    assert(b.check(ResourceCategory::Book, 2, Money::of(200)).empty());
    assert(!b.check(ResourceCategory::Book, 6, Money::of(10)).empty());   // units
    assert(!b.check(ResourceCategory::Book, 1, Money::of(401)).empty());  // spend
    assert(!b.check(ResourceCategory::ElectronicResource, 1, Money::of(1001)).empty());  // overall
    assert(b.check(ResourceCategory::ElectronicResource, 1, Money::of(900)).empty());    // no quota

    b.commit(ResourceCategory::Book, 4, Money::of(300));
    assert(b.spent() == Money::of(300));
    assert(*b.unitsRemaining(ResourceCategory::Book) == 1);
    assert(*b.spendRemaining(ResourceCategory::Book) == Money::of(100));
    assert(!b.unitsRemaining(ResourceCategory::ElectronicResource).has_value());

    assert((throwsExpected<QuotaExceededError>([&] { (void)(b.commit(ResourceCategory::Book, 2, Money::of(10))); })));
    assert((throwsExpected<BudgetExceededError>([&] { (void)(b.commit(ResourceCategory::ElectronicResource, 1, Money::of(800))); })));
    assert((throwsExpected<std::invalid_argument>([&] { (void)(b.commit(ResourceCategory::ElectronicResource, 0, Money::of(1))); })));
    assert(b.spent() == Money::of(300));  // failed commits changed nothing
}

static void testAcquisition() {
    Catalog c;
    c.emplace<Book>("B1", "Book", std::vector<std::string>{"A"}, "i", "P", 2020,
                    Money::of(100));
    c.emplace<ElectronicResource>("R1", "DB", "P", 2026, Money::of(10), "url",
                                  LicenseModel::AnnualSubscription, Money::of(50));
    Budget b(Money::of(500));
    b.setQuota(ResourceCategory::Book, {3, Money::of(1000)});
    AcquisitionManager acq(c, b);

    assert(acq.quote("R1", 5) == Money::of(100));
    std::string why;
    assert(acq.canPurchase("B1", 3, &why) && why.empty());
    assert(!acq.canPurchase("B1", 4, &why) && !why.empty());
    assert(!acq.canPurchase("nope", 1, &why));

    const auto& rec = acq.purchase("B1", 2);
    assert(rec.approved && rec.cost == Money::of(200) && rec.orderNo == 1);
    assert(c.holdings("B1") == 2);

    assert((throwsExpected<QuotaExceededError>([&] { (void)(acq.purchase("B1", 2)); })));
    assert((throwsExpected<NotFoundError>([&] { (void)(acq.purchase("nope", 1)); })));
    assert(acq.history().size() == 1);  // exceptions don't record

    auto res = acq.processBatch({{"R1", 10}, {"R1", 100}, {"B1", 1}, {"zzz", 1}, {"B1", 0}});
    assert(res.size() == 5);
    assert(res[0].approved && res[0].cost == Money::of(150));
    assert(!res[1].approved);  // 1050 > remaining 150
    assert(res[2].approved);
    assert(!res[3].approved && res[3].reason.find("not found") != std::string::npos);
    assert(!res[4].approved);
    assert(acq.totalSpent() == Money::of(450));
    assert(b.spent() == acq.totalSpent());
    assert(c.holdings("R1") == 10 && c.holdings("B1") == 3);
    assert(acq.history().size() == 6);
}

// Part A, Q1: subscription pricing, validation and journal quotas.
static void testJournal() {
    Journal annual("J0", "Annual", "1111-2222", 4, "P", 2026, Money::of(100));
    assert(annual.subscriptionYears() == 1);
    assert(annual.costFor(2) == Money::of(200));
    assert(!annual.isDigital());
    assert(annual.issn() == "1111-2222" && annual.issuesPerYear() == 4);
    assert((throwsExpected<std::invalid_argument>([&] { (void)(annual.costFor(0)); })));
    assert((throwsExpected<std::invalid_argument>([&] { (void)(annual.costFor(-1)); })));
    assert((throwsExpected<std::invalid_argument>([&] { (void)(Journal("J", "T", "i", 4, "P", 2026, Money::of(1), 0)); })));
    assert((throwsExpected<std::invalid_argument>([&] { (void)(Journal("J", "T", "i", 4, "P", 2026, Money::of(1), -1)); })));
    assert((throwsExpected<std::invalid_argument>([&] { (void)(Journal("J", "T", "i", 0, "P", 2026, Money::of(1))); })));
    Catalog c;
    c.emplace<Journal>("J1", "Journal", "1234-5678", 12, "P", 2026, Money::of(100), 3);
    const Resource& r = c.get("J1");
    assert(r.category() == ResourceCategory::Journal);
    assert(r.costFor(2) == Money::of(600));
    assert(c.byCategory(ResourceCategory::Journal).size() == 1);
    std::ostringstream details;
    details << r;
    assert(details.str().find("issn: 1234-5678") != std::string::npos);
    assert(details.str().find("issues per year: 12") != std::string::npos);
    assert(details.str().find("subscription years: 3") != std::string::npos);
    Budget b(Money::of(2000));
    b.setQuota(ResourceCategory::Journal, {2, Money::of(600)});
    AcquisitionManager acq(c, b);
    acq.purchase("J1", 2);
    assert(c.holdings("J1") == 2 && b.spent() == Money::of(600));
    assert((throwsExpected<QuotaExceededError>([&] { (void)(acq.purchase("J1", 1)); })));
    assert(c.holdings("J1") == 2 && b.spent() == Money::of(600));
    std::ostringstream report;
    b.print(report);
    assert(report.str().find("Journal") != std::string::npos);
    assert(report.str().find("600.00/600.00") != std::string::npos);
    Budget spendLimited(Money::of(2000));
    spendLimited.setQuota(ResourceCategory::Journal, {10, Money::of(599)});
    assert((throwsExpected<QuotaExceededError>([&] { (void)(spendLimited.commit(r.category(), 2, r.costFor(2))); })));
    assert(spendLimited.spent() == Money{});
}

// Part A, Q2: inherited electronic pricing and separate e-book quotas.
static void testEBook() {
    Catalog c;
    auto& e = c.emplace<EBook>("E1", "Digital Book", std::vector<std::string>{"A", "B"},
        "isbn", "P", 2026, Money::of(10), "url", FileFormat::EPUB, true,
        LicenseModel::Perpetual, Money::of(50));
    const Resource& r = e;
    assert(r.isDigital() && r.category() == ResourceCategory::EBook);
    assert(r.costFor(3) == Money::of(80));
    assert(e.authors().size() == 2 && e.isbn() == "isbn");
    assert(e.format() == FileFormat::EPUB && e.drmProtected());
    assert(e.license() == LicenseModel::Perpetual && e.accessUrl() == "url");
    assert((throwsExpected<std::invalid_argument>([&] { (void)(r.costFor(0)); })));
    assert((throwsExpected<std::invalid_argument>([&] { (void)(r.costFor(-1)); })));
    std::ostringstream details;
    details << r;
    const auto text = details.str();
    assert(text.find("platform fee: 50.00") < text.find("authors: A and B"));
    assert(text.find("access url: url") != std::string::npos);
    assert(text.find("license: Perpetual") != std::string::npos);
    assert(text.find("isbn: isbn") != std::string::npos);
    assert(text.find("file format: EPUB") != std::string::npos);
    assert(text.find("drm protected: yes") != std::string::npos);
    for (FileFormat format : {FileFormat::PDF, FileFormat::EPUB, FileFormat::HTML}) {
        EBook other("E", "T", {}, "i", "P", 2026, Money::of(10), "url", format, false);
        assert(other.costFor(2) == Money::of(20));
        std::ostringstream os;
        os << other;
        assert(os.str().find(fileFormatName(format)) != std::string::npos);
        assert(os.str().find("drm protected: no") != std::string::npos);
    }
    assert((throwsExpected<std::invalid_argument>([&] { (void)(EBook("E", "T", {}, "i", "P", 2026, Money::of(1), "url",
                      static_cast<FileFormat>(99), false)); })));
    c.emplace<ElectronicResource>("R1", "DB", "P", 2026, Money::of(10), "url");
    Budget b(Money::of(1000));
    b.setQuota(ResourceCategory::EBook, {3, Money::of(80)});
    b.setQuota(ResourceCategory::ElectronicResource, {1, Money::of(10)});
    AcquisitionManager acq(c, b);
    acq.purchase("E1", 3);
    assert((throwsExpected<QuotaExceededError>([&] { (void)(acq.purchase("E1", 1)); })));
    acq.purchase("R1", 1); // Exhausting EBook quotas does not consume the DB quota.
    assert(b.usageFor(ResourceCategory::EBook).units == 3);
    assert(b.usageFor(ResourceCategory::ElectronicResource).units == 1);
    assert(c.byCategory(ResourceCategory::EBook).size() == 1);
    assert(c.holdings("E1") == 3);
    std::ostringstream report;
    b.print(report);
    assert(report.str().find("EBook") != std::string::npos);
}

// Part A, Q3: digital audiobook and print thesis through Resource interfaces.
static void testAudioBookAndThesis() {
    Catalog c;
    auto& audio = c.emplace<AudioBook>("A1", "Audio", "P", 2026, Money::of(10), "url",
                                     "Narrator", 90, LicenseModel::Perpetual, Money::of(5));
    auto& thesis = c.emplace<Thesis>("T1", "Research", "IIITD", "PhD", "Supervisor", 2026);
    assert(audio.narrator() == "Narrator" && audio.durationMinutes() == 90);
    assert(audio.category() == ResourceCategory::AudioBook && audio.isDigital());
    const Resource& a = audio;
    assert(a.costFor(2) == Money::of(25));
    assert((throwsExpected<std::invalid_argument>([&] { (void)(a.costFor(0)); })));
    assert((throwsExpected<std::invalid_argument>([&] { (void)(AudioBook("A", "T", "P", 2026, Money{}, "url", "N", 0)); })));
    assert((throwsExpected<std::invalid_argument>([&] { (void)(AudioBook("A", "T", "P", 2026, Money{}, "url", "N", -1)); })));
    assert(thesis.category() == ResourceCategory::Thesis && !thesis.isDigital());
    assert(thesis.university() == "IIITD" && thesis.degree() == "PhD");
    assert(thesis.supervisor() == "Supervisor");
    const Resource& t = thesis;
    assert(t.costFor(3) == Money{});
    assert((throwsExpected<std::invalid_argument>([&] { (void)(t.costFor(0)); })));
    Thesis paid("T2", "Paid", "U", "MSc", "S", 2026, Money::of(5));
    assert(paid.costFor(2) == Money::of(10));
    std::ostringstream details;
    details << a << t;
    assert(details.str().find("narrator: Narrator") != std::string::npos);
    assert(details.str().find("duration in minutes: 90") != std::string::npos);
    assert(details.str().find("platform fee: 5.00") != std::string::npos);
    assert(details.str().find("university: IIITD") != std::string::npos);
    assert(details.str().find("degree: PhD") != std::string::npos);
    assert(details.str().find("supervisor: Supervisor") != std::string::npos);
    Budget b(Money::of(100));
    b.setQuota(ResourceCategory::AudioBook, {2, Money::of(25)});
    b.setQuota(ResourceCategory::Thesis, {1, Money{}});
    AcquisitionManager acq(c, b);
    acq.purchase("A1", 2);
    acq.purchase("T1", 1);
    assert((throwsExpected<QuotaExceededError>([&] { (void)(acq.purchase("T1", 1)); })));
    assert(c.holdings("A1") == 2 && c.holdings("T1") == 1);
    assert(b.spent() == Money::of(25));
    assert(c.all().size() == 2);
    std::ostringstream report;
    b.print(report);
    assert(report.str().find("AudioBook") != std::string::npos);
    assert(report.str().find("Thesis") != std::string::npos);
}

// Part B, Q4: hardcover surcharge; listed unit price stays unchanged.
static void testHardcoverPricing() {
    Book hard("H", "Hard", {}, "i", "P", 2026, Money::of(100), 1, Binding::Hardcover);
    Book paper("P", "Paper", {}, "i", "P", 2026, Money::of(100));
    const Resource& r = hard;
    assert(r.costFor(1) == Money::of(120));
    assert(r.costFor(3) == Money::of(360));
    assert(hard.unitPrice() == Money::of(100));
    assert(paper.costFor(3) == Money::of(300));
    assert((throwsExpected<std::invalid_argument>([&] { (void)(hard.costFor(0)); })));
    assert((throwsExpected<std::invalid_argument>([&] { (void)(hard.costFor(-1)); })));
    hard.setUnitPrice(Money::fromMinor(3));
    assert(hard.costFor(1) == Money::fromMinor(4)); // 3.6 rounds upward.
}

// Part B, Q5: boundaries and inherited rules for all resource types.
static void testBulkPricing() {
    Book paper("P", "Paper", {}, "i", "P", 2026, Money::of(100));
    Book hard("H", "Hard", {}, "i", "P", 2026, Money::of(100), 1, Binding::Hardcover);
    assert(paper.costFor(9) == Money::of(900));
    assert(paper.costFor(10) == Money::of(900));
    assert(paper.costFor(11) == Money::of(990));
    assert(hard.costFor(9) == Money::of(1080));
    assert(hard.costFor(10) == Money::of(1080));
    assert(hard.costFor(11) == Money::of(1188));
    Journal journal("J", "Journal", "i", 12, "P", 2026, Money::of(100), 3);
    assert(journal.costFor(9) == Money::of(2700));
    assert(journal.costFor(10) == Money::of(2700));
    Thesis thesis("T", "Thesis", "U", "PhD", "S", 2026, Money::of(10));
    assert(thesis.costFor(10) == Money::of(90));
    Thesis free("F", "Free", "U", "PhD", "S", 2026);
    assert(free.costFor(10) == Money{});
    ElectronicResource db("D", "DB", "P", 2026, Money::of(10), "url",
                          LicenseModel::Perpetual, Money::of(100));
    assert(db.costFor(49) == Money::of(590));
    assert(db.costFor(50) == Money::of(600));
    assert(db.costFor(51) == Money::of(605));
    assert(db.costFor(60) == Money::of(650));
    EBook ebook("E", "EBook", {}, "i", "P", 2026, Money::of(10), "url",
                FileFormat::PDF, false, LicenseModel::Perpetual, Money::of(100));
    AudioBook audio("A", "Audio", "P", 2026, Money::of(10), "url", "N", 90,
                    LicenseModel::Perpetual, Money::of(100));
    assert(ebook.costFor(51) == db.costFor(51));
    assert(audio.costFor(60) == db.costFor(60));
    ElectronicResource tiny("M", "Tiny", "P", 2026, Money::fromMinor(1), "url");
    assert(tiny.costFor(51) == Money::fromMinor(51));
    assert(tiny.costFor(52) == Money::fromMinor(51)); // Discounted subtotal rounded once.
    assert((throwsExpected<std::invalid_argument>([&] { (void)(db.costFor(-1)); })));
    // Separate purchases do not combine to earn a bulk discount.
    assert(paper.costFor(5) + paper.costFor(5) == Money::of(1000));
}

// Part B, Q6: quote, approval and batch paths use the same post-tax amount.
static void testTaxes() {
    Catalog c;
    c.emplace<Book>("B", "Book", std::vector<std::string>{}, "i", "P", 2026, Money::of(100));
    c.emplace<ElectronicResource>("D", "DB", "P", 2026, Money::of(10), "url",
                                  LicenseModel::Perpetual, Money::of(100));
    c.emplace<Journal>("J", "Journal", "i", 12, "P", 2026, Money::of(100), 2);
    c.emplace<EBook>("E", "EBook", std::vector<std::string>{}, "i", "P", 2026,
                     Money::of(10), "url", FileFormat::PDF, false);
    c.emplace<AudioBook>("A", "Audio", "P", 2026, Money::of(10), "url", "N", 90);
    c.emplace<Thesis>("T", "Thesis", "U", "PhD", "S", 2026);
    Budget b(Money::of(5000));
    AcquisitionManager acq(c, b);
    assert(acq.quote("B", 1) == Money::of(100)); // Zero tax default.
    acq.setTaxRates({1000, 2000}); // 10% print; 20% electronic.
    c.emplace<Book>("H", "Hard", std::vector<std::string>{}, "i", "P", 2026,
                    Money::of(100), 1, Binding::Hardcover);
    assert(acq.quoteBeforeTax("H", 10) == Money::of(1080));
    assert(acq.quote("H", 10) == Money::of(1188));
    assert(acq.quoteBeforeTax("B", 10) == Money::of(900));
    assert(acq.quote("B", 10) == Money::of(990));
    assert(acq.quoteBeforeTax("D", 60) == Money::of(650));
    assert(acq.quote("D", 60) == Money::of(780)); // Fee is taxed too.
    assert(acq.quote("J", 1) == Money::of(220));
    assert(acq.quote("E", 1) == Money::of(12));
    assert(acq.quote("A", 1) == Money::of(12));
    assert(acq.quote("T", 1) == Money{});
    assert((throwsExpected<std::invalid_argument>([&] { (void)(acq.setTaxRates({-1, 2000})); })));
    assert(acq.taxRates().printBasisPoints == 1000);
    assert((throwsExpected<std::invalid_argument>([&] { (void)(AcquisitionManager(c, b, {0, -1})); })));
    b.setQuota(ResourceCategory::Book, {20, Money::of(100)});
    std::string why;
    assert(!acq.canPurchase("B", 1, &why)); // Pre-tax fits, post-tax does not.
    assert(why.find("spend quota") != std::string::npos);
    assert((throwsExpected<QuotaExceededError>([&] { (void)(acq.purchase("B", 1)); })));
    assert(c.holdings("B") == 0 && b.spent() == Money{});
    auto rejected = acq.processBatch({{"B", 1}, {"missing", 1}, {"B", 0}});
    assert(!rejected[0].approved && rejected[0].preTaxCost == Money::of(100));
    assert(rejected[0].cost == Money::of(110));
    assert(rejected[1].preTaxCost == Money{} && rejected[2].cost == Money{});
    assert(b.spent() == Money{} && c.holdings("B") == 0);
    b.setQuota(ResourceCategory::Book, {20, Money::of(110)});
    const auto record = acq.purchase("B", 1);
    assert(record.preTaxCost == Money::of(100) && record.cost == Money::of(110));
    assert(b.usageFor(ResourceCategory::Book).spent == Money::of(110));
    assert(acq.totalBeforeTax() == Money::of(100) && acq.totalSpent() == Money::of(110));
    acq.setTaxRates({500, 750});
    assert(acq.quote("B", 1) == Money::of(105));
    assert(acq.history().back().cost == Money::of(110)); // History preserves old rate.
    std::ostringstream report;
    acq.printReport(report);
    assert(report.str().find("pre-tax: 100.00, post-tax: 110.00") != std::string::npos);
    assert(report.str().find("Total pre-tax: 100.00") != std::string::npos);
    assert(report.str().find("Total post-tax (spent): 110.00") != std::string::npos);
    Budget small(Money::of(109));
    AcquisitionManager limited(c, small, {1000, 0});
    assert(!limited.canPurchase("B", 1));
    assert((throwsExpected<BudgetExceededError>([&] { (void)(limited.purchase("B", 1)); })));
    assert(small.spent() == Money{});
    Budget electronicBudget(Money::of(1000));
    electronicBudget.setQuota(ResourceCategory::ElectronicResource, {60, Money::of(650)});
    AcquisitionManager electronic(c, electronicBudget, {0, 2000});
    assert(!electronic.processBatch({{"D", 60}})[0].approved);
    electronicBudget.setQuota(ResourceCategory::ElectronicResource, {60, Money::of(780)});
    assert(electronic.processBatch({{"D", 60}})[0].approved);
    assert(electronicBudget.spent() == Money::of(780));
    c.emplace<Book>("tiny", "Tiny", std::vector<std::string>{}, "i", "P", 2026,
                    Money::fromMinor(5));
    acq.setTaxRates({1000, 0});
    assert(acq.quote("tiny", 1) == Money::fromMinor(6)); // Half a paise rounds up.
}

// Part C, Q7: count catalogue IDs, not copies or repeated purchases.
static void testTitleQuotas() {
    Catalog c;
    for (const auto& id : {"B001", "B002", "B003"})
        c.emplace<Book>(id, "Same displayed title", std::vector<std::string>{}, "i", "P",
                        2026, Money::of(10));
    c.emplace<Journal>("J001", "Journal", "issn", 12, "P", 2026, Money::of(10));
    Budget b(Money::of(1000));
    b.setQuota(ResourceCategory::Book, {30, Money::of(1000), 2});
    b.setQuota(ResourceCategory::Journal, {10, Money::of(100), 1});
    AcquisitionManager acq(c, b);
    acq.purchase("B001", 2);
    acq.purchase("B001", 1);
    assert(b.usageFor(ResourceCategory::Book).titles == 1);
    assert(*b.titlesRemaining(ResourceCategory::Book) == 1);
    assert(acq.canPurchase("B002", 1));
    assert(b.usageFor(ResourceCategory::Book).titles == 1); // Query has no effects.
    acq.purchase("B002", 1);
    assert(b.usageFor(ResourceCategory::Book).titles == 2);
    std::string reason;
    assert(!acq.canPurchase("B003", 1, &reason));
    assert(reason.find("title quota") != std::string::npos);
    assert((throwsExpected<QuotaExceededError>([&] { (void)(acq.purchase("B003", 1)); })));
    assert(c.holdings("B003") == 0);
    assert(acq.canPurchase("B001", 1)); // More of an existing title is permitted.
    const auto batch = acq.processBatch({{"B003", 1}, {"B002", 1}, {"J001", 1}});
    assert(!batch[0].approved && batch[1].approved && batch[2].approved);
    assert(b.usageFor(ResourceCategory::Journal).titles == 1);
    assert(b.usageFor(ResourceCategory::Book).titles == 2);
    assert((throwsExpected<std::invalid_argument>([&] { (void)(b.setQuota(ResourceCategory::Book, {1, Money::of(10), -1})); })));
    Budget zero(Money::of(100));
    zero.setQuota(ResourceCategory::Book, {10, Money::of(100), 0});
    assert((throwsExpected<QuotaExceededError>([&] { (void)(zero.commit(ResourceCategory::Book, 1, Money::of(1), "B001")); })));
    assert((throwsExpected<std::invalid_argument>([&] { (void)(zero.commit(ResourceCategory::Book, 1, Money::of(1))); })));
    assert(zero.spent() == Money{});
    b.removeQuota(ResourceCategory::Book);
    assert(!b.titlesRemaining(ResourceCategory::Book));
    acq.purchase("B003", 1);
    assert(b.usageFor(ResourceCategory::Book).titles == 3); // Track even without a limit.
    std::ostringstream os;
    b.print(os);
    assert(os.str().find("Titles used/max") != std::string::npos);
    Budget oneTitle(Money::of(1000));
    oneTitle.setQuota(ResourceCategory::Book, {20, Money::of(1000), 1});
    AcquisitionManager batchManager(c, oneTitle);
    const auto crossing = batchManager.processBatch({{"B001", 1}, {"B002", 1}, {"B001", 1}});
    assert(crossing[0].approved && !crossing[1].approved && crossing[2].approved);
    assert(oneTitle.usageFor(ResourceCategory::Book).titles == 1);
}

// Part C, Q8: refund recorded totals and preserve the original order history.
static void testCancellation() {
    Catalog c;
    c.emplace<Book>("B001", "Book", std::vector<std::string>{}, "i", "P", 2026, Money::of(100));
    c.emplace<Book>("B002", "Other", std::vector<std::string>{}, "i", "P", 2026, Money::of(100));
    c.emplace<Thesis>("T001", "Free", "U", "PhD", "S", 2026);
    Budget b(Money::of(3000));
    b.setQuota(ResourceCategory::Book, {20, Money::of(2000), 1});
    b.setQuota(ResourceCategory::Thesis, {1, Money{}, 1});
    AcquisitionManager acq(c, b, {1000, 0});
    const auto first = acq.purchase("B001", 10); // Discounted: 900 + 90 tax.
    const auto second = acq.purchase("B001", 1);
    assert(b.spent() == Money::of(1100));
    c.get("B001").setUnitPrice(Money::of(200));
    acq.setTaxRates({2000, 0});
    const auto cancellation = acq.cancel(first.orderNo);
    assert(cancellation.cancellation && cancellation.cancelledOrderNo == first.orderNo);
    assert(cancellation.cost == Money::of(990) && cancellation.preTaxCost == Money::of(900));
    assert(b.spent() == Money::of(110) && c.holdings("B001") == 1);
    assert(b.usageFor(ResourceCategory::Book).titles == 1); // One active order remains.
    assert(acq.history()[0].approved && !acq.history()[0].cancellation);
    assert(acq.history()[0].cost == first.cost && acq.history()[0].quantity == 10);
    assert(acq.totalSpent() == b.spent() && acq.totalBeforeTax() == Money::of(100));
    const auto before = acq.history().size();
    assert((throwsExpected<std::invalid_argument>([&] { (void)(acq.cancel(first.orderNo)); })));
    assert((throwsExpected<std::invalid_argument>([&] { (void)(acq.cancel(cancellation.orderNo)); })));
    assert((throwsExpected<std::invalid_argument>([&] { (void)(acq.cancel(999)); })));
    assert(acq.history().size() == before && b.spent() == Money::of(110));
    acq.cancel(second.orderNo);
    assert(c.holdings("B001") == 0 && b.spent() == Money{});
    assert(b.usageFor(ResourceCategory::Book).titles == 0);
    assert(*b.unitsRemaining(ResourceCategory::Book) == 20);
    assert(*b.spendRemaining(ResourceCategory::Book) == Money::of(2000));
    assert(acq.canPurchase("B002", 1)); // Freed title slot.
    const auto free = acq.purchase("T001", 1);
    acq.cancel(free.orderNo);
    assert(c.holdings("T001") == 0 && b.usageFor(ResourceCategory::Thesis).titles == 0);
    auto rejected = acq.processBatch({{"missing", 1}})[0];
    assert((throwsExpected<std::invalid_argument>([&] { (void)(acq.cancel(rejected.orderNo)); })));
    auto held = acq.purchase("B002", 2);
    c.addHoldings("B002", -1); // Simulate holdings no longer available for full cancellation.
    assert((throwsExpected<std::invalid_argument>([&] { (void)(acq.cancel(held.orderNo)); })));
    assert(b.spent() == held.cost && c.holdings("B002") == 1);
    c.addHoldings("B002", 1);
    acq.cancel(held.orderNo);
    assert(b.spent() == Money{} && acq.totalSpent() == Money{});
    std::ostringstream os;
    acq.printReport(os);
    assert(os.str().find("CANCELLED") != std::string::npos);
    assert(os.str().find("refund of order #1") != std::string::npos);
    assert((throwsExpected<std::invalid_argument>([&] { (void)(b.refund(ResourceCategory::Book, 1, Money::of(1), "B002")); })));
    c.emplace<ElectronicResource>("R001", "DB", "P", 2026, Money::of(10), "url",
                                  LicenseModel::Perpetual, Money::of(100));
    acq.setTaxRates({0, 2000});
    const auto electronic = acq.purchase("R001", 60);
    assert(electronic.preTaxCost == Money::of(650) && electronic.cost == Money::of(780));
    acq.cancel(electronic.orderNo);
    assert(b.spent() == Money{} && c.holdings("R001") == 0);
    assert(b.usageFor(ResourceCategory::ElectronicResource).titles == 0);
    const auto removed = acq.purchase("B002", 1);
    c.remove("B002");
    assert((throwsExpected<NotFoundError>([&] { (void)(acq.cancel(removed.orderNo)); })));
    assert(b.spent() == removed.cost && !acq.history().back().cancellation);
}

// Part C, Q9: independent departments, correct rejection routing and refunds.
static void testDepartments() {
    Catalog c;
    c.emplace<Book>("B001", "Book", std::vector<std::string>{}, "i", "P", 2026, Money::of(100));
    c.emplace<Book>("B002", "Other", std::vector<std::string>{}, "i", "P", 2026, Money::of(100));
    Budget defaultBudget(Money::of(1000)), cs(Money::of(500)), physics(Money::of(110));
    cs.setQuota(ResourceCategory::Book, {4, Money::of(500), 1});
    physics.setQuota(ResourceCategory::Book, {2, Money::of(500), 1});
    AcquisitionManager acq(c, defaultBudget, {1000, 0});
    acq.addDepartment("Computer Science", cs);
    acq.addDepartment("Physics", physics);
    assert((throwsExpected<std::invalid_argument>([&] { (void)(acq.addDepartment("Computer Science", physics)); })));
    assert((throwsExpected<std::invalid_argument>([&] { (void)(acq.addDepartment("", cs)); })));
    assert((throwsExpected<std::invalid_argument>([&] { (void)(acq.addDepartment("Other", cs)); })));
    assert((throwsExpected<std::invalid_argument>([&] { (void)(acq.addDepartment("Default", defaultBudget)); })));
    assert((throwsExpected<std::invalid_argument>([&] { (void)(acq.departmentBudget("unknown")); })));
    auto batch = acq.processBatch({{"B001", 2, "Computer Science"}, {"B001", 1, "Physics"},
                                  {"B001", 1, "Physics"}, {"B002", 1, "Computer Science"},
                                  {"B001", 1, "unknown"}});
    assert(batch[0].approved && batch[1].approved);
    assert(!batch[2].approved && batch[2].reason.find("overall budget") != std::string::npos);
    assert(!batch[3].approved && batch[3].reason.find("title quota") != std::string::npos);
    assert(!batch[4].approved && batch[4].reason.find("unknown department") != std::string::npos);
    assert(batch[0].department == "Computer Science" && batch[4].department == "unknown");
    assert(cs.spent() == Money::of(220) && physics.spent() == Money::of(110));
    assert(defaultBudget.spent() == Money{} && c.holdings("B001") == 3);
    std::string why;
    assert(!acq.canPurchase("B001", 1, &why, "unknown"));
    assert((throwsExpected<std::invalid_argument>([&] { (void)(acq.purchase("B001", 1, "unknown")); })));
    acq.cancel(batch[0].orderNo);
    assert(cs.spent() == Money{} && physics.spent() == Money::of(110));
    assert(c.holdings("B001") == 1);
    assert(acq.history().back().department == "Computer Science");
    assert(acq.canPurchase("B002", 1, nullptr, "Computer Science"));
    acq.purchase("B002", 1, "Computer Science");
    acq.purchase("B001", 1); // Existing API still charges the default budget.
    assert(defaultBudget.spent() == Money::of(110));
    assert(acq.totalSpent() == cs.spent() + physics.spent() + defaultBudget.spent());
    std::ostringstream report;
    acq.printReport(report);
    assert(report.str().find("department: Physics") != std::string::npos);
}

// Part C, Q10: carry forward unspent money, copy quotas and reset usage.
static void testRollover() {
    Budget b(Money::of(1000));
    b.setQuota(ResourceCategory::Book, {10, Money::of(1000), 2});
    b.commit(ResourceCategory::Book, 9, Money::of(810), "B001");
    Budget next = b.rollover(Money::of(2000), 50);
    assert(next.total() == Money::of(2095)); // Half of unspent 190.
    assert(next.spent() == Money{} && next.usageFor(ResourceCategory::Book).units == 0);
    assert(next.usageFor(ResourceCategory::Book).titles == 0);
    assert(next.quotaFor(ResourceCategory::Book)->maxTitles == 2);
    assert(next.quotaFor(ResourceCategory::Book)->maxUnits == 10);
    assert(next.quotaFor(ResourceCategory::Book)->maxSpend == Money::of(1000));
    next.commit(ResourceCategory::Book, 1, Money::of(10), "B002");
    assert(b.spent() == Money::of(810) && b.usageFor(ResourceCategory::Book).titles == 1);
    assert(b.rollover(Money::of(2000), 0).total() == Money::of(2000));
    assert(b.rollover(Money::of(2000), 100).total() == Money::of(2190));
    assert((throwsExpected<std::invalid_argument>([&] { (void)(b.rollover(Money::of(1), -1)); })));
    assert((throwsExpected<std::invalid_argument>([&] { (void)(b.rollover(Money::of(1), 101)); })));
    assert((throwsExpected<std::invalid_argument>([&] { (void)(b.rollover(Money::fromMinor(-1), 50)); })));
    Budget tiny(Money::fromMinor(5));
    assert(tiny.rollover(Money{}, 50).total() == Money::fromMinor(3));
    Budget maximum(Money::fromMinor(std::numeric_limits<std::int64_t>::max()));
    assert((throwsExpected<std::overflow_error>([&] { (void)(maximum.rollover(Money::fromMinor(1), 100)); })));
    assert(maximum.rollover(Money{}, 100).total() == maximum.total());
}

// Part D, Q11: atomic batches check cumulative usage and preserve existing state.
static void testAtomicBatch() {
    Catalog c;
    c.emplace<Book>("B001", "Book", std::vector<std::string>{}, "i", "P", 2026, Money::of(100));
    c.emplace<Book>("B002", "Other", std::vector<std::string>{}, "i", "P", 2026, Money::of(100));
    Budget b(Money::of(1000));
    b.setQuota(ResourceCategory::Book, {5, Money::of(800), 1});
    AcquisitionManager acq(c, b, {1000, 0});
    const auto existing = acq.purchase("B001", 1);
    auto failed = acq.processBatch({{"B001", 3}, {"B001", 2}}, true);
    assert(failed.size() == 2 && !failed[0].approved && !failed[1].approved);
    assert(failed[0].reason.find("atomic batch aborted") != std::string::npos);
    assert(failed[1].reason.find("unit quota") != std::string::npos);
    assert(b.spent() == Money::of(110) && c.holdings("B001") == 1);
    assert(b.usageFor(ResourceCategory::Book).units == 1);
    assert(b.usageFor(ResourceCategory::Book).titles == 1);
    assert(acq.history().size() == 3 && acq.history()[0].orderNo == existing.orderNo);
    assert(acq.totalSpent() == Money::of(110));
    auto success = acq.processBatch({{"B001", 2}, {"B001", 2}}, true);
    assert(success[0].approved && success[1].approved);
    assert(success[0].orderNo == 4 && success[1].orderNo == 5);
    assert(b.spent() == Money::of(550) && c.holdings("B001") == 5);
    acq.cancel(success[0].orderNo); // Atomic purchases remain cancellable individually.
    assert(b.spent() == Money::of(330) && c.holdings("B001") == 3);
    const auto historySize = acq.history().size();
    assert(acq.processBatch({}, true).empty());
    assert(acq.history().size() == historySize);
    auto invalid = acq.processBatch({{"B001", 1}, {"missing", 1}, {"B001", 0},
                                     {"B001", 1, "unknown"}}, true);
    assert(!invalid[0].approved && !invalid[1].approved && !invalid[2].approved && !invalid[3].approved);
    assert(invalid[1].reason.find("not found") != std::string::npos);
    assert(invalid[2].reason.find("quantity") != std::string::npos);
    assert(invalid[3].reason.find("unknown department") != std::string::npos);
    assert(b.spent() == Money::of(330) && c.holdings("B001") == 3);
    assert((throwsExpected<std::invalid_argument>([&] { (void)(acq.cancel(invalid[0].orderNo)); })));
    Budget titleBudget(Money::of(1000));
    titleBudget.setQuota(ResourceCategory::Book, {10, Money::of(1000), 1});
    AcquisitionManager titles(c, titleBudget);
    auto distinct = titles.processBatch({{"B001", 1}, {"B002", 1}}, true);
    assert(!distinct[0].approved && !distinct[1].approved);
    assert(distinct[1].reason.find("title quota") != std::string::npos);
    assert(titleBudget.usageFor(ResourceCategory::Book).titles == 0);
    Budget spending(Money::of(1000));
    spending.setQuota(ResourceCategory::Book, {10, Money::of(200)});
    AcquisitionManager taxed(c, spending, {1000, 0});
    assert(!taxed.processBatch({{"B001", 1}, {"B001", 1}}, true)[1].approved);
    assert(spending.spent() == Money{}); // 200 pre-tax fits, 220 post-tax does not.
    Budget total(Money::of(150));
    AcquisitionManager limited(c, total);
    assert(!limited.processBatch({{"B001", 1}, {"B001", 1}}, true)[0].approved);
    assert(total.spent() == Money{});
    Budget partialBudget(Money::of(150));
    AcquisitionManager partial(c, partialBudget);
    const auto ordinary = partial.processBatch({{"B001", 1}, {"B001", 1}});
    assert(ordinary[0].approved && !ordinary[1].approved);
    assert(partialBudget.spent() == Money::of(100));
}

static void testAtomicDepartments() {
    Catalog c;
    c.emplace<Book>("B001", "Book", std::vector<std::string>{}, "i", "P", 2026, Money::of(100));
    Budget defaultBudget(Money::of(1000)), cs(Money::of(500)), physics(Money::of(150));
    AcquisitionManager acq(c, defaultBudget);
    acq.addDepartment("Computer Science", cs);
    acq.addDepartment("Physics", physics);
    auto failed = acq.processBatch({{"B001", 1, "Computer Science"}, {"B001", 2, "Physics"}}, true);
    assert(!failed[0].approved && !failed[1].approved);
    assert(cs.spent() == Money{} && physics.spent() == Money{} && c.holdings("B001") == 0);
    auto success = acq.processBatch({{"B001", 2, "Computer Science"}, {"B001", 1, "Physics"},
                                     {"B001", 1}}, true);
    assert(success[0].approved && success[1].approved && success[2].approved);
    assert(cs.spent() == Money::of(200) && physics.spent() == Money::of(100));
    assert(defaultBudget.spent() == Money::of(100) && c.holdings("B001") == 4);
    acq.cancel(success[1].orderNo);
    assert(physics.spent() == Money{} && cs.spent() == Money::of(200));
}

// Part D, Q12: vendor prices obey the same polymorphic pricing and tax rules.
static void testVendors() {
    Catalog c;
    c.emplace<Book>("B001", "Hardcover", std::vector<std::string>{}, "i", "P", 2026,
                    Money::of(100), 1, Binding::Hardcover);
    c.emplace<Journal>("J001", "Journal", "issn", 12, "P", 2026, Money::of(100), 2);
    c.emplace<ElectronicResource>("R001", "DB", "P", 2026, Money::of(10), "url",
                                  LicenseModel::Perpetual, Money::of(100));
    c.emplace<EBook>("E001", "EBook", std::vector<std::string>{}, "i", "P", 2026,
                     Money::of(10), "url", FileFormat::EPUB, true,
                     LicenseModel::Perpetual, Money::of(50));
    c.emplace<AudioBook>("A001", "Audio", "P", 2026, Money::of(10), "url", "N", 90,
                         LicenseModel::Perpetual, Money::of(30));
    c.emplace<Thesis>("T001", "Thesis", "U", "PhD", "S", 2026, Money::of(10));
    Budget b(Money::of(10000));
    AcquisitionManager acq(c, b, {1000, 2000});
    assert(acq.quoteBeforeTax("B001", 1) == Money::of(120)); // No offers: catalogue pricing.
    c.setVendorPrice("B001", "Expensive", Money::of(90));
    c.setVendorPrice("B001", "Zeta", Money::of(80));
    c.setVendorPrice("B001", "Alpha", Money::of(80)); // Alphabetical tie break.
    assert(c.vendors("B001").size() == 3);
    assert(acq.quoteBeforeTax("B001", 10) == Money::of(864));
    assert(acq.quote("B001", 10) == Money::of(950, 40));
    assert(c.get("B001").unitPrice() == Money::of(100)); // Quote does not mutate the object.
    const auto hard = acq.purchase("B001", 10);
    assert(hard.vendor == "Alpha" && hard.preTaxCost == Money::of(864));
    assert(hard.cost == Money::of(950, 40));
    c.setVendorPrice("B001", "Alpha", Money::of(70));
    assert(c.vendors("B001").size() == 3); // Update, not duplicate vendor.
    assert(acq.quoteBeforeTax("B001", 1) == Money::of(84));
    assert(acq.history()[0].vendor == "Alpha" && acq.history()[0].cost == hard.cost);
    c.setVendorPrice("J001", "Journal Vendor", Money::of(60));
    c.setVendorPrice("R001", "Database Vendor", Money::of(5));
    c.setVendorPrice("E001", "EBook Vendor", Money::of(8));
    c.setVendorPrice("A001", "Audio Vendor", Money::of(6));
    c.setVendorPrice("T001", "Thesis Vendor", Money::of(4));
    assert(acq.quoteBeforeTax("J001", 10) == Money::of(1080));
    assert(acq.quoteBeforeTax("R001", 60) == Money::of(375));
    assert(acq.quoteBeforeTax("E001", 60) == Money::of(490));
    assert(acq.quoteBeforeTax("A001", 60) == Money::of(360));
    assert(acq.quoteBeforeTax("T001", 10) == Money::of(36));
    auto purchased = acq.processBatch({{"J001", 10}, {"R001", 60}, {"E001", 60},
                                      {"A001", 60}, {"T001", 10}}, true);
    for (const auto& order : purchased) assert(order.approved && !order.vendor.empty());
    assert(purchased[1].cost == Money::of(450));
    acq.setTaxRates({0, 0});
    acq.cancel(hard.orderNo);
    assert(acq.history().back().vendor == "Alpha" && acq.history().back().cost == hard.cost);
    assert(c.holdings("B001") == 0);
    assert((throwsExpected<std::invalid_argument>([&] { (void)(c.setVendorPrice("B001", "", Money::of(1))); })));
    assert((throwsExpected<std::invalid_argument>([&] { (void)(c.setVendorPrice("B001", "Bad", Money::fromMinor(-1))); })));
    assert((throwsExpected<NotFoundError>([&] { (void)(c.setVendorPrice("missing", "V", Money::of(1))); })));
    assert((throwsExpected<NotFoundError>([&] { (void)(c.vendors("missing")); })));
    assert((throwsExpected<std::invalid_argument>([&] { (void)(c.get("B001").costAtUnitPrice(1, Money::fromMinor(-1))); })));
    assert((throwsExpected<std::invalid_argument>([&] { (void)(c.get("R001").costAtUnitPrice(0, Money::of(1))); })));
    std::ostringstream report;
    acq.printReport(report);
    assert(report.str().find("vendor: Alpha") != std::string::npos);
    assert(report.str().find("vendor: Journal Vendor") != std::string::npos);
    c.remove("T001");
    c.emplace<Thesis>("T001", "New", "U", "PhD", "S", 2026);
    assert(c.vendors("T001").empty()); // Offers belong to the removed resource.
}

static void testVendorQuotaChecks() {
    Catalog c;
    c.emplace<Book>("B001", "Book", std::vector<std::string>{}, "i", "P", 2026, Money::of(100));
    c.setVendorPrice("B001", "Cheap", Money::of(50));
    c.setVendorPrice("B001", "Other", Money::of(70));
    Budget b(Money::of(110));
    b.setQuota(ResourceCategory::Book, {4, Money::of(110), 1});
    AcquisitionManager acq(c, b, {1000, 0});
    assert(acq.canPurchase("B001", 2)); // Vendor makes a previously unaffordable request fit.
    auto failed = acq.processBatch({{"B001", 2}, {"B001", 1}}, true);
    assert(!failed[0].approved && !failed[1].approved && b.spent() == Money{});
    assert(failed[0].vendor == "Cheap" && failed[0].cost == Money::of(110));
    auto partial = acq.processBatch({{"B001", 2}, {"B001", 1}});
    assert(partial[0].approved && !partial[1].approved);
    assert(partial[0].vendor == "Cheap" && partial[1].vendor == "Cheap");
    assert(b.spent() == Money::of(110));
    c.setVendorPrice("B001", "Cheap", Money{});
    assert(acq.quote("B001", 1) == Money{}); // Zero-price offers are legitimate.
}

static void populatePartECatalog(Catalog& c) {
    c.emplace<Book>("B001", "C++ \"Design\"\nNotes", std::vector<std::string>{"Ada Lovelace", "ADA Research"},
                    "isbn-1", "Publisher\\Press", 2020, Money::of(100), 3, Binding::Hardcover);
    c.emplace<Journal>("J001", "Journal", "issn-1", 12, "P", 2021, Money::of(50), 2);
    c.emplace<ElectronicResource>("R001", "DB", "P", 2022, Money::of(10), "url",
                                  LicenseModel::Perpetual, Money::of(100));
    c.emplace<EBook>("E001", "EBook", std::vector<std::string>{"Ada Lovelace"}, "isbn-2",
                     "P", 2023, Money::of(20), "url", FileFormat::EPUB, true,
                     LicenseModel::AnnualSubscription, Money::of(50));
    c.emplace<AudioBook>("A001", "Audio", "P", 2024, Money::of(15), "url", "Narrator", 180,
                         LicenseModel::Perpetual, Money::of(30));
    c.emplace<Thesis>("T001", "Thesis", "IIITD", "PhD", "Supervisor", 2025, Money::of(10));
    for (const Resource* r : c.all()) {
        c.addHoldings(r->id(), 3);
        c.setVendorPrice(r->id(), "Vendor \"One\"", Money::of(5));
        c.setVendorPrice(r->id(), "Vendor Two", Money::of(7));
    }
}

// Part E, Q13: author substring, exact ISBN/ISSN and inclusive year searches.
static void testCatalogSearches() {
    Catalog c;
    populatePartECatalog(c);
    auto authors = c.searchAuthor("aDa");
    assert(authors.size() == 2); // Multiple matching authors do not duplicate a resource.
    assert(authors[0]->id() == "B001" && authors[1]->id() == "E001");
    assert(c.searchAuthor("LOVELACE").size() == 2);
    assert(c.searchAuthor("missing").empty());
    assert(c.searchIdentifier("isbn-1")[0]->id() == "B001");
    assert(c.searchIdentifier("isbn-2")[0]->id() == "E001");
    assert(c.searchIdentifier("issn-1")[0]->id() == "J001");
    assert(c.searchIdentifier("isbn").empty()); // Exact identifier, not a substring.
    assert(c.searchIdentifier("").empty());
    assert(c.byYearRange(2021, 2023).size() == 3);
    assert(c.byYearRange(2020, 2020)[0]->id() == "B001");
    assert(c.byYearRange(2030, 2040).empty());
    assert((throwsExpected<std::invalid_argument>([&] { (void)(c.byYearRange(2025, 2020)); })));
    assert(c.size() == 6 && c.holdings("B001") == 3);
}

// Part E, Q14: one patron/copy or patron/seat per resource, across all categories.
static void testLending() {
    Catalog c;
    populatePartECatalog(c);
    for (const Resource* r : c.all()) {
        const auto id = r->id();
        const bool digital = r->isDigital();
        if (digital) c.openSession(id, "P001");
        else c.borrow(id, "P001");
        assert(c.available(id) == 2 && c.holdings(id) == 3);
        assert((throwsExpected<LendingError>([&] { (void)(c.remove(id)); })));
        assert((throwsExpected<LendingError>([&] { (void)(c.addHoldings(id, -3)); })));
        assert(c.holdings(id) == 3);
        if (digital) {
            assert(c.sessionUsers(id).size() == 1);
            assert((throwsExpected<LendingError>([&] { (void)(c.openSession(id, "P001")); })));
            assert((throwsExpected<LendingError>([&] { (void)(c.borrow(id, "P002")); })));
            assert((throwsExpected<LendingError>([&] { (void)(c.returnCopy(id, "P001")); })));
            assert((throwsExpected<std::invalid_argument>([&] { (void)(c.openSession(id, "")); })));
            assert((throwsExpected<LendingError>([&] { (void)(c.closeSession(id, "P999")); })));
            c.openSession(id, "P002"); c.openSession(id, "P003");
            assert((throwsExpected<LendingError>([&] { (void)(c.openSession(id, "P004")); })));
            c.closeSession(id, "P001"); c.openSession(id, "P004");
            c.closeSession(id, "P002"); c.closeSession(id, "P003"); c.closeSession(id, "P004");
        } else {
            assert(c.borrowers(id).size() == 1);
            assert((throwsExpected<LendingError>([&] { (void)(c.borrow(id, "P001")); })));
            assert((throwsExpected<LendingError>([&] { (void)(c.openSession(id, "P002")); })));
            assert((throwsExpected<LendingError>([&] { (void)(c.closeSession(id, "P001")); })));
            assert((throwsExpected<std::invalid_argument>([&] { (void)(c.borrow(id, "")); })));
            assert((throwsExpected<LendingError>([&] { (void)(c.returnCopy(id, "P999")); })));
            c.borrow(id, "P002"); c.borrow(id, "P003");
            assert((throwsExpected<LendingError>([&] { (void)(c.borrow(id, "P004")); })));
            c.returnCopy(id, "P001"); c.borrow(id, "P004");
            c.returnCopy(id, "P002"); c.returnCopy(id, "P003"); c.returnCopy(id, "P004");
        }
        assert(c.available(id) == 3 && c.holdings(id) == 3);
    }
    assert((throwsExpected<NotFoundError>([&] { (void)(c.borrow("missing", "P001")); })));
    assert((throwsExpected<NotFoundError>([&] { (void)(c.available("missing")); })));
    Catalog emptyHoldings;
    emptyHoldings.emplace<Thesis>("T001", "Thesis", "U", "PhD", "S", 2026);
    assert((throwsExpected<LendingError>([&] { (void)(emptyHoldings.borrow("T001", "P001")); })));
    c.remove("B001"); // Once all copies are returned, removal is allowed.
    assert(!c.contains("B001"));
}

static void testLendingAndCancellation() {
    Catalog c;
    c.emplace<Book>("B001", "Book", std::vector<std::string>{}, "i", "P", 2026, Money::of(100));
    c.emplace<ElectronicResource>("R001", "DB", "P", 2026, Money::of(10), "url");
    Budget b(Money::of(1000));
    AcquisitionManager acq(c, b);
    const auto book = acq.purchase("B001", 2);
    const auto electronic = acq.purchase("R001", 2);
    c.borrow("B001", "P001");
    c.openSession("R001", "P001");
    const auto count = acq.history().size();
    assert((throwsExpected<std::invalid_argument>([&] { (void)(acq.cancel(book.orderNo)); })));
    assert((throwsExpected<std::invalid_argument>([&] { (void)(acq.cancel(electronic.orderNo)); })));
    assert(b.spent() == Money::of(220) && acq.history().size() == count);
    // Atomic acquisitions can add capacity while resources are in use.
    auto orders = acq.processBatch({{"B001", 1}, {"R001", 1}}, true);
    assert(orders[0].approved && orders[1].approved);
    assert(c.available("B001") == 2 && c.available("R001") == 2);
    acq.cancel(orders[0].orderNo); acq.cancel(orders[1].orderNo);
    c.returnCopy("B001", "P001"); c.closeSession("R001", "P001");
    acq.cancel(book.orderNo); acq.cancel(electronic.orderNo);
    assert(b.spent() == Money{} && c.holdings("B001") == 0 && c.holdings("R001") == 0);
}

// Part F, Q15: every mixed-currency arithmetic/comparison must throw, even zero.
static void testMoneyCurrencies() {
    const Money usd = Money::of(10, 50, "USD");
    const Money eur = Money::of(10, 50, "EUR");
    assert(Money{}.currency() == "INR");
    assert(Money::of(1).currency() == "INR");
    assert(usd.currency() == "USD" && usd.toString() == "10.50");
    assert(Money::fromMinor(1050, "USD") == usd);
    assert(usd + Money::of(2, 0, "USD") == Money::of(12, 50, "USD"));
    assert(usd - Money::of(2, 0, "USD") == Money::of(8, 50, "USD"));
    assert(usd * 2 == Money::of(21, 0, "USD"));
    assert(2 * usd == Money::of(21, 0, "USD"));
    assert(Money::of(-3, 50, "USD").toString() == "-3.50");
    assert(usd == Money::of(10, 50, "USD"));
    assert(usd != Money::of(10, 0, "USD"));
    assert(usd > Money::of(10, 0, "USD"));
    assert(usd >= Money::of(10, 50, "USD"));
    assert(usd < Money::of(11, 0, "USD"));
    assert(usd <= Money::of(10, 50, "USD"));
    assert((throwsExpected<CurrencyMismatchError>([&] { (void)(usd + eur); })));
    assert((throwsExpected<CurrencyMismatchError>([&] { (void)(usd - eur); })));
    assert((throwsExpected<CurrencyMismatchError>([&] { (void)(usd == eur); })));
    assert((throwsExpected<CurrencyMismatchError>([&] { (void)(usd != eur); })));
    assert((throwsExpected<CurrencyMismatchError>([&] { (void)(usd < eur); })));
    assert((throwsExpected<CurrencyMismatchError>([&] { (void)(usd <= eur); })));
    assert((throwsExpected<CurrencyMismatchError>([&] { (void)(usd > eur); })));
    assert((throwsExpected<CurrencyMismatchError>([&] { (void)(usd >= eur); })));
    Money unchanged = usd;
    assert((throwsExpected<CurrencyMismatchError>([&] { (void)(unchanged += eur); })));
    assert(unchanged == usd);
    assert((throwsExpected<CurrencyMismatchError>([&] { (void)(unchanged -= eur); })));
    assert(unchanged == usd);
    assert((throwsExpected<CurrencyMismatchError>([&] { (void)(Money::fromMinor(0, "USD") + Money{}); })));
    assert((throwsExpected<CurrencyMismatchError>([&] { (void)(Money::fromMinor(0, "USD") == Money{}); })));
    assert((throwsExpected<std::invalid_argument>([&] { (void)(Money::of(1, 0, "usd")); })));
    assert((throwsExpected<std::invalid_argument>([&] { (void)(Money::fromMinor(1, "US")); })));
    assert((throwsExpected<std::invalid_argument>([&] { (void)(Money::fromMinor(1, "")); })));
    assert((throwsExpected<std::invalid_argument>([&] { (void)(Money::fromMinor(1, "U1D")); })));
}

static void testForeignCurrencyPricing() {
    const auto dollars = [](int amount) { return Money::of(amount, 0, "USD"); };
    Catalog c;
    c.emplace<Book>("B001", "Book", std::vector<std::string>{}, "i", "P", 2027,
                    dollars(100), 1, Binding::Hardcover);
    c.emplace<Journal>("J001", "Journal", "issn", 12, "P", 2027, dollars(50), 2);
    c.emplace<ElectronicResource>("R001", "DB", "P", 2027, dollars(10), "url",
                                  LicenseModel::Perpetual, dollars(100));
    c.emplace<EBook>("E001", "EBook", std::vector<std::string>{}, "i", "P", 2027,
                     dollars(20), "url", FileFormat::PDF, false,
                     LicenseModel::Perpetual, dollars(50));
    c.emplace<AudioBook>("A001", "Audio", "P", 2027, dollars(15), "url", "N", 180,
                         LicenseModel::Perpetual, dollars(30));
    c.emplace<Thesis>("T001", "Thesis", "U", "PhD", "S", 2027, dollars(10));
    ElectronicResource omitted("R", "DB", "P", 2027, dollars(10), "url");
    assert(omitted.platformFee() == dollars(0));
    assert(omitted.costFor(1) == dollars(10));
    EBook ebook("E", "EBook", {}, "i", "P", 2027, dollars(10), "url", FileFormat::PDF, false);
    AudioBook audio("A", "Audio", "P", 2027, dollars(10), "url", "N", 90);
    assert(ebook.costFor(1) == dollars(10) && audio.costFor(1) == dollars(10));
    assert((throwsExpected<CurrencyMismatchError>([&] { (void)(ElectronicResource("R", "DB", "P", 2027, dollars(10), "url",
                                   LicenseModel::Perpetual, Money{})); })));
    Budget b(dollars(10000));
    for (const Resource* r : c.all()) b.setQuota(r->category(), {100, dollars(5000), 2});
    assert(b.spent() == dollars(0));
    assert(b.usageFor(ResourceCategory::Book).spent == dollars(0));
    AcquisitionManager acq(c, b, {1000, 2000});
    assert(acq.totalSpent() == dollars(0));
    assert(acq.quoteBeforeTax("B001", 10) == dollars(1080));
    assert(acq.quote("B001", 10) == dollars(1188));
    assert(acq.quote("J001", 10) == dollars(990));
    assert(acq.quote("R001", 60) == dollars(780));
    assert(acq.quote("E001", 60) == dollars(1380));
    assert(acq.quote("A001", 60) == dollars(1026));
    assert(acq.quote("T001", 10) == dollars(99));
    c.setVendorPrice("B001", "USD Vendor", dollars(80));
    assert(acq.quote("B001", 10) == Money::of(950, 40, "USD"));
    assert((throwsExpected<CurrencyMismatchError>([&] { (void)(c.setVendorPrice("B001", "Wrong", Money::of(50))); })));
    assert((throwsExpected<CurrencyMismatchError>([&] { (void)(c.get("B001").setUnitPrice(Money::of(100))); })));
    assert((throwsExpected<CurrencyMismatchError>([&] { (void)(b.setQuota(ResourceCategory::Book, {100, Money::of(1000)})); })));
    assert((throwsExpected<std::invalid_argument>([&] { (void)(b.commit(ResourceCategory::Book, 1, Money::of(100), "B001")); })));
    assert(b.spent() == dollars(0));
    auto purchases = acq.processBatch({{"B001", 10}, {"J001", 10}, {"R001", 60},
                                       {"E001", 60}, {"A001", 60}, {"T001", 10}}, true);
    for (const auto& record : purchases) assert(record.approved && record.cost.currency() == "USD");
    const Money spent = b.spent();
    acq.cancel(purchases[0].orderNo);
    assert(b.spent() == spent - purchases[0].cost);
    assert(acq.totalSpent() == b.spent());
    assert(b.usageFor(ResourceCategory::Book).spent == dollars(0));
    assert((throwsExpected<CurrencyMismatchError>([&] { (void)(b.refund(ResourceCategory::Journal, 1, Money::of(1), "J001")); })));
    Budget next = b.rollover(dollars(10000), 50);
    assert(next.total().currency() == "USD" && next.spent() == dollars(0));
    assert(next.quotaFor(ResourceCategory::Book)->maxSpend.currency() == "USD");
    assert((throwsExpected<CurrencyMismatchError>([&] { (void)(b.rollover(Money::of(10000), 50)); })));
    std::ostringstream report;
    acq.printReport(report); b.print(report);
    assert(report.str().find("currency: USD") != std::string::npos);
    assert(report.str().find("Currency: USD") != std::string::npos);
}

static void testMixedCurrencyDepartments() {
    Catalog c;
    c.emplace<Book>("B001", "INR Book", std::vector<std::string>{}, "i", "P", 2027, Money::of(100));
    c.emplace<Book>("B002", "USD Book", std::vector<std::string>{}, "i", "P", 2027,
                    Money::of(10, 0, "USD"));
    Budget india(Money::of(1000)), us(Money::of(100, 0, "USD"));
    AcquisitionManager acq(c, india);
    acq.addDepartment("US", us);
    assert(!acq.canPurchase("B002", 1));
    auto failed = acq.processBatch({{"B001", 1}, {"B002", 1}}, true);
    assert(!failed[0].approved && !failed[1].approved);
    assert(india.spent() == Money{} && us.spent() == Money::of(0, 0, "USD"));
    auto success = acq.processBatch({{"B001", 2}, {"B002", 3, "US"}}, true);
    assert(success[0].approved && success[1].approved);
    assert(india.spent() == Money::of(200) && us.spent() == Money::of(30, 0, "USD"));
    assert((throwsExpected<CurrencyMismatchError>([&] { (void)(acq.totalSpent()); })));
    assert((throwsExpected<CurrencyMismatchError>([&] { (void)(acq.totalBeforeTax()); })));
    const auto totals = acq.totalsByCurrency();
    assert(totals.at("INR") == Money::of(200));
    assert(totals.at("USD") == Money::of(30, 0, "USD"));
    std::ostringstream report;
    acq.printReport(report);
    assert(report.str().find("Total post-tax (spent): 200.00 INR") != std::string::npos);
    assert(report.str().find("Total post-tax (spent): 30.00 USD") != std::string::npos);
    auto rejected = acq.processBatch({{"B001", 1, "US"}});
    assert(!rejected[0].approved && us.spent() == Money::of(30, 0, "USD"));
}

// Part G: expected-exception helper must reject absent and unrelated exceptions.
static void testExceptionAssertions() {
    assert(throwsExpected<std::invalid_argument>([] { throw std::invalid_argument("expected"); }));
    assert(!throwsExpected<std::invalid_argument>([] {}));
    assert(!throwsExpected<std::invalid_argument>([] { throw std::runtime_error("wrong type"); }));
}

int main() {
    testExceptionAssertions();
    testMoney();
    testResourcesAndCost();
    testCatalog();
    testBudget();
    testAcquisition();
    testJournal();
    testEBook();
    testAudioBookAndThesis();
    testHardcoverPricing();
    testBulkPricing();
    testTaxes();
    testTitleQuotas();
    testCancellation();
    testDepartments();
    testRollover();
    testAtomicBatch();
    testAtomicDepartments();
    testVendors();
    testVendorQuotaChecks();
    testCatalogSearches();
    testLending();
    testLendingAndCancellation();
    testMoneyCurrencies();
    testForeignCurrencyPricing();
    testMixedCurrencyDepartments();
    std::cout << "All test groups passed.\n";
    return 0;
}
