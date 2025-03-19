import std;
import kqm.str;

using namespace std::literals;


#define TEST(x)                                                                                                        \
    do {                                                                                                               \
        std::println("Test: {}", #x);                                                                                  \
        x();                                                                                                           \
        std::println();                                                                                                \
    } while (0)

auto fmt(char32_t ch)
{
    auto buf = std::array<char, 5>{};
    auto len = kqm::utf::utf32_to_utf8(ch, buf.data());
    return std::string{buf.data(), len};
}

auto test_default_ctor()
{
    auto s = kqm::string{};
    std::println("default ctor: {:?}", s);
}

auto test_std_sv_ctor()
{
    auto s = kqm::string{"sv"sv};
    std::println("from std::sv: {:?}", s);
}

auto test_range_ctor()
{
    auto chs = std::vector{U'r', U'a', U'n', U'g', U'e'};
    auto s   = kqm::string{chs.begin(), chs.end()};
    std::println("from range: {:?}", s);
}

auto test_sv_ctor()
{
    auto s = kqm::string{"test"sv};
    std::println("from kqm::sv: {:?}", s);
}

auto test_move_ctor()
{
    auto s  = kqm::string{"short string"};
    auto s2 = std::move(s);
    std::println("move: {:?} -> {:?}", s, s2);

    s = kqm::string{"large capacity"};
    s.reserve(32);
    s2 = std::move(s);
    std::println("move: {:?} -> {:?}", s, s2);
}

auto test_front()
{
    auto s  = kqm::string{"string"};
    auto ch = s.front();
    std::println("{:?}.front() == U's' is {}", s, ch == U's');
}

auto test_back()
{
    auto s  = kqm::string{"string"};
    auto ch = s.back();
    std::println("{:?}.back() == U'g' is {}", s, ch == U'g');
}

auto test_range()
{
    auto s = kqm::string{"string"};
    std::print("{} ->", s);
    for (auto ch : s) {
        std::print(" {:?}", fmt(ch));
    }
    std::println();
}

auto test_reverse_range()
{
    auto s = kqm::string{"string"};
    std::print("{} | reverse ->", s);
    for (auto ch : s | std::views::reverse) {
        std::print(" {:?}", fmt(ch));
    }
    std::println();
}

auto test_clear()
{
    auto s       = kqm::string{"string"};
    auto old_siz = s.size_bytes();
    s.clear();
    std::println("size_bytes(): {} -> {}", old_siz, s.size_bytes());
}

auto test_append()
{
    auto s = kqm::string{"string"};
    std::println("   {:?}", s);
    s.append(U';');
    std::println("-> {:?}", s);
    s.append(" append"sv);
    std::println("-> {:?}", s);
    s.append(kqm::string_view{" a"});
    std::println("-> {:?}", s);
    s.append(kqm::string{" new"});
    std::println("-> {:?}", s);
    s += " section"sv;
    std::println("-> {:?}", s);
}

auto test_replace()
{
    auto s     = kqm::string{};
    auto first = kqm::string::iterator{};
    auto last  = kqm::string::iterator{};
    auto init  = [&] {
        s     = kqm::string{"AAABBBCCC"};
        first = std::next(s.begin(), 3);
        last  = std::next(first, 3);
    };
    init();
    std::println("   {:?}", s);
    s.replace(first, last, U'D');
    std::println("-> {:?}", s);
    init();
    s.replace(first, last, "DD"sv);
    std::println("-> {:?}", s);
    init();
    s.replace(first, last, kqm::string_view{"DDDD"});
    std::println("-> {:?}", s);
}

auto test_assign()
{
    auto s = kqm::string{"string"};
    std::println("   {:?}", s);
    s.assign(U's');
    std::println("-> {:?}", s);
    s.assign("std::sv"sv);
    std::println("-> {:?}", s);
    s.assign(kqm::string_view{"sv"});
    std::println("-> {:?}", s);
    s.assign(kqm::string{"str"});
    std::println("-> {:?}", s);
    s += "eq"sv;
    std::println("-> {:?}", s);
}

auto main() -> int
{
    TEST(test_default_ctor);
    TEST(test_std_sv_ctor);
    TEST(test_range_ctor);
    TEST(test_sv_ctor);
    TEST(test_move_ctor);

    TEST(test_front);
    TEST(test_back);
    TEST(test_range);
    TEST(test_reverse_range);

    TEST(test_clear);
    TEST(test_append);
    TEST(test_replace);
    TEST(test_assign);
}
