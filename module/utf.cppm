export module kqm.str.utf;

import std;

template <typename To, typename From>
struct tag_limit
{
    using type = From;
};

template <typename To, std::derived_from<To> From>
struct tag_limit<To, From>
{
    using type = To;
};

template <typename From, typename To>
using tag_limit_t = tag_limit<To, From>::type;

export namespace kqm
{

namespace utf
{

template <typename T>
concept Utf8CharOrByteType = std::same_as<T, std::byte> || std::same_as<T, char> || std::same_as<T, char8_t>;

template <typename T>
concept Utf32CharType = std::same_as<T, char32_t> || std::same_as<T, std::uint32_t>;

template <typename In>
concept Utf8ByteIterator = std::input_iterator<In>
                           && (std::convertible_to<std::iter_value_t<In>, std::byte>
                               || Utf8CharOrByteType<std::remove_cvref_t<std::iter_value_t<In>>>);

template <Utf32CharType Char>
constexpr auto is_valid_codepoint(Char codepoint) noexcept
{
    const auto cp = static_cast<std::uint32_t>(codepoint);

    return !(cp > 0x10'FFFF || (cp >= 0xD800 && cp <= 0xDFFF));
}

template <Utf32CharType Char>
constexpr auto is_valid_utf8_byte(Char byte) noexcept
{
    const auto value = static_cast<std::uint8_t>(byte);

    if (value == 0xC0) return false;
    if (value == 0xC1) return false;
    if (value >= 0xF5) return false;
    return true;
}

template <Utf32CharType Char>
constexpr auto utf8_length_from_utf32(Char codepoint) noexcept -> std::size_t
{
    const auto cp = static_cast<std::uint32_t>(codepoint);

    if (cp <= 0x7F) {
        return 1uz;
    } else if (cp <= 0x7FF) {
        return 2uz;
    } else if (cp <= 0xFFFF) {
        return 3uz;
    } else {
        return 4uz;
    }
}

template <Utf8CharOrByteType Byte>
constexpr auto utf8_lead_byte_length(Byte lead) noexcept -> std::size_t
{
    auto byte = static_cast<std::uint8_t>(lead);

    if (byte < 0x80) return 1uz;
    if ((byte & 0xE0) == 0xC0) return 2uz;
    if ((byte & 0xF0) == 0xE0) return 3uz;
    return 4uz;
}

template <std::input_or_output_iterator Out>
    requires Utf8CharOrByteType<std::remove_reference_t<std::iter_reference_t<Out>>>
             && std::output_iterator<Out, std::remove_reference_t<std::iter_reference_t<Out>>>
auto utf32_to_utf8(char32_t codepoint, Out out) noexcept
{
    using Byte = std::remove_reference_t<std::iter_reference_t<Out>>;

    const auto cp = static_cast<std::uint32_t>(codepoint);

    if (cp <= 0x7F) {
        *out = static_cast<Byte>(cp);
    } else if (cp <= 0x7FF) {
        *out = static_cast<Byte>(0xC0 | (cp >> 6));
        ++out;
        *out = static_cast<Byte>(0x80 | (cp & 0x3F));
    } else if (cp <= 0xFFFF) {
        *out = static_cast<Byte>(0xE0 | (cp >> 12));
        ++out;
        *out = static_cast<Byte>(0x80 | ((cp >> 6) & 0x3F));
        ++out;
        *out = static_cast<Byte>(0x80 | (cp & 0x3F));
    } else {
        *out = static_cast<Byte>(0xF0 | (cp >> 18));
        ++out;
        *out = static_cast<Byte>(0x80 | ((cp >> 12) & 0x3F));
        ++out;
        *out = static_cast<Byte>(0x80 | ((cp >> 6) & 0x3F));
        ++out;
        *out = static_cast<Byte>(0x80 | (cp & 0x3F));
    }
    ++out;
    return out;
}

template <Utf8ByteIterator In>
constexpr auto utf8_to_utf32(In in) noexcept -> std::pair<char32_t, In>
{
    const auto lead = static_cast<std::uint8_t>(*in);

    auto ret = char32_t{};
    if (lead < 0x80) {
        ret = lead;
    } else if ((lead & 0xE0) == 0xC0) {
        ret = (lead & 0x1F) << 6;
        ++in;
        ret |= static_cast<std::uint8_t>(*in) & 0x3F;
    } else if ((lead & 0xF0) == 0xE0) {
        ret = (lead & 0x0F) << 12;
        ++in;
        ret |= (static_cast<std::uint8_t>(*in) & 0x3F) << 6;
        ++in;
        ret |= (static_cast<std::uint8_t>(*in) & 0x3F);
    } else {
        ret = (lead & 0x0F) << 18;
        ++in;
        ret |= (static_cast<std::uint8_t>(*in) & 0x3F) << 12;
        ++in;
        ret |= (static_cast<std::uint8_t>(*in) & 0x3F) << 6;
        ++in;
        ret |= (static_cast<std::uint8_t>(*in) & 0x3F);
    }
    ++in;
    return {ret, in};
}

template <Utf8ByteIterator Iter>
struct utf8_iterator
{
private:
    Iter iter_;

    using base_trait = std::iterator_traits<Iter>;

    template <typename Tag>
    using limiter = tag_limit_t<Tag, std::bidirectional_iterator_tag>;

public:
    using iterator_concept  = limiter<typename base_trait::iterator_concept>;
    using iterator_category = limiter<typename base_trait::iterator_category>;
    using value_type        = char32_t;
    using difference_type   = std::ptrdiff_t;

    explicit constexpr utf8_iterator() noexcept = default;

    explicit constexpr utf8_iterator(Iter iter) noexcept : iter_{iter} {}

    constexpr auto operator*() const noexcept -> char32_t
    {
        return utf8_to_utf32(iter_).first;
    }

    constexpr auto operator++(this utf8_iterator& self) noexcept -> utf8_iterator&
    {
        std::advance(self.iter_, utf8_lead_byte_length(*self.iter_));
        return self;
    }

    constexpr auto operator++(this utf8_iterator self, int) noexcept -> utf8_iterator
    {
        ++self;
        return self;
    }

    constexpr auto operator--(this utf8_iterator& self) noexcept -> utf8_iterator&
        requires std::bidirectional_iterator<Iter>
    {
        do {
            --self.iter_;
        } while ((static_cast<std::uint8_t>(*self.iter_) & 0xC0) == 0x80);
        return self;
    }

    constexpr auto operator--(this utf8_iterator self, int) noexcept -> utf8_iterator
        requires std::bidirectional_iterator<Iter>
    {
        --self;
        return self;
    }

    constexpr auto base() const noexcept -> const Iter&
    {
        return iter_;
    }
};

template <typename T, typename U>
constexpr auto operator==(const utf8_iterator<T>& lhs, const utf8_iterator<U>& rhs)
    noexcept(noexcept(lhs.base() == rhs.base())) -> bool
    requires std::equality_comparable_with<T, U>
{
    return lhs.base() == rhs.base();
}

template <typename T, typename U>
constexpr auto operator<=>(const utf8_iterator<T>& lhs, const utf8_iterator<U>& rhs)
    noexcept(noexcept(lhs.base() <=> rhs.base()))
    requires std::three_way_comparable_with<T, U>
{
    return lhs.base() <=> rhs.base();
}

constexpr auto iterate_utf8(Utf8ByteIterator auto iter) noexcept
{
    return utf8_iterator{std::move(iter)};
}

constexpr auto iterate_utf8(Utf8ByteIterator auto first, Utf8ByteIterator auto last) noexcept
{
    return std::ranges::subrange{iterate_utf8(std::move(first)), iterate_utf8(std::move(last))};
}

template <Utf8ByteIterator Iter, std::sentinel_for<Iter> Sentinel>
    requires(!Utf8ByteIterator<Sentinel>)
constexpr auto iterate_utf8(Iter first, Sentinel last) noexcept
{
    return std::ranges::subrange{iterate_utf8(std::move(first)), std::move(last)};
}

}

}

struct as_utf32_t : std::ranges::range_adaptor_closure<as_utf32_t>
{
    static constexpr auto operator()(std::ranges::range auto&& r) noexcept
    {
        return kqm::utf::iterate_utf8(std::ranges::begin(std::forward_like<decltype(r)>(r)),
                                      std::ranges::end(std::forward_like<decltype(r)>(r)));
    }
};


export namespace kqm
{

namespace ranges::views
{

constexpr auto as_utf32 = as_utf32_t{};

}

namespace views = ranges::views;

}
