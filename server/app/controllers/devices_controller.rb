class DevicesController < ApplicationController
  before_action :set_device, only: [:show, :log, :update, :destroy]

  def index
    @devices = @user.devices.includes(:app).all
  end

  def show
  end

  def create
    @device = @user.devices.new(device_params)
    @device.save!
    render :show, status: :created, location: @device
  end

  def update
    app = params.dig(:device, :app)
    if app
      @device.app = @user.apps.find_by_name!(app)
    end

    @device.update!(device_params)
    render :show, status: :ok, location: @device
  end

  def destroy
    @device.destroy
  end

  private

  def set_device
    @device = @user.devices.find_by_name!(params[:name])
  end

  def device_params
    params.require(:device).permit(:name, :tag, :device_type, :sakuraio_module_token)
  end
end
