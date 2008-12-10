require "test/unit"

$:.unshift File.dirname(__FILE__) + "/../ext/strophe_ruby"

class TestStropheRubyExtn < Test::Unit::TestCase
  def test_truth
    assert true
  end
end
