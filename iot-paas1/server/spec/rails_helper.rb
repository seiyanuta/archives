# This file is copied to spec/ when you run 'rails generate rspec:install'
require 'spec_helper'
ENV['RAILS_ENV'] ||= 'test'
require File.expand_path('../../config/environment', __FILE__)
# Prevent database truncation if the environment is production
abort("The Rails environment is running in production mode!") if Rails.env.production?

require 'simplecov'

SimpleCov.profiles.define 'makestack' do
  load_profile 'rails'
  add_filter 'vendor'
end

SimpleCov.start 'makestack'

require 'rspec/rails'
require 'rspec/json_expectations'
require 'ffaker'
require 'database_cleaner'
require 'webmock/rspec'

%w(* shared_examples/*).each do |pattern|
  Dir[Rails.root.join("spec/support/#{pattern}.rb")].each do |f|
    require f
  end
end

# Checks for pending migration and applies them before tests are run.
# If you are not using ActiveRecord, you can remove this line.
ActiveRecord::Migration.maintain_test_schema!

Shoulda::Matchers.configure do |config|
  config.integrate do |with|
    with.test_framework :rspec
    with.library :rails
  end
end

RSpec.configure do |config|
  config.include Devise::Test::ControllerHelpers, type: :controller
  config.include ControllerHelpers, type: :controller
  config.include RoutingHelpers, type: :routing
  config.include ActiveJob::TestHelper

  config.infer_base_class_for_anonymous_controllers = false
  config.render_views = true
  config.fixture_path = "#{::Rails.root}/spec/fixtures"
  config.use_transactional_fixtures = true
  config.infer_spec_type_from_file_location!
  config.filter_rails_from_backtrace!

  WebMock.disable_net_connect!

  config.before(:suite) do
    DatabaseCleaner.strategy = :transaction
    DatabaseCleaner.clean_with(:truncation)
  end

  config.around(:each) do |example|
    DatabaseCleaner.cleaning do
      example.run
    end
  end

  config.around(:each) do |example|
    clean_redis do
      example.run
    end
  end

  def clean_redis(&block)
    Redis.__current.flushall
    yield
  rescue
    Redis.__current.flushall
  end
end
